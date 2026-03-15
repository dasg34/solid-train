#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any
from urllib.parse import urlencode, quote

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import KST, fetch_text, format_korean_datetime
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


SPEC = ScenarioSpec(
    skill_name="tv-commute-briefing",
    title="TV Commute Briefing",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="commute_main",
    loading_title="출근길 준비 중",
    loading_detail="목적지와 예상 소요 시간을 계산하고 있습니다.",
    loading_hint="추천 출발 시각과 대안 경로를 먼저 채웁니다.",
    empty_title="표시할 이동 정보 없음",
    empty_detail="다음 일정이나 목적지가 아직 없습니다.",
    empty_hint="캘린더 목적지 또는 mock 경로 정보를 먼저 연결해 주세요.",
    error_title="이동 정보를 불러오지 못했습니다",
    error_detail="경로 API 또는 위치 정규화 상태를 확인해 주세요.",
    error_hint="정확한 주소 노출 대신 요약된 위치 표현을 유지하는 편이 안전합니다.",
    retry_event="refreshCommute",
)

DEFAULT_ORIGIN = "서울시청"
DEFAULT_DESTINATION = "강남역"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV commute briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "osrm"],
        default="mock",
        help="Choose whether to use the bundled mock file or fetch live route data via Nominatim and OSRM.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SPEC.default_input,
        help="Path to a normalized scenario JSON file used for --source mock.",
    )
    parser.add_argument(
        "--surface-id",
        default=SPEC.default_surface_id,
        help="Surface ID to use in emitted A2UI messages.",
    )
    parser.add_argument(
        "--catalog-id",
        default="https://a2ui.org/specification/v0_9/standard_catalog.json",
        help="Catalog ID to use in the createSurface message.",
    )
    parser.add_argument(
        "--origin",
        default=DEFAULT_ORIGIN,
        help="Origin query used when --source osrm is selected.",
    )
    parser.add_argument(
        "--destination",
        default=DEFAULT_DESTINATION,
        help="Destination query used when --source osrm is selected.",
    )
    parser.add_argument(
        "--origin-label",
        help="Optional masked label for the origin shown on TV.",
    )
    parser.add_argument(
        "--destination-label",
        help="Optional masked label for the destination shown on TV.",
    )
    parser.add_argument(
        "--profile",
        choices=["driving", "walking"],
        default="driving",
        help="OSRM profile used for the primary route.",
    )
    parser.add_argument(
        "--arrive-by",
        help="Optional ISO-8601 arrival target in Asia/Seoul, e.g. 2026-03-15T09:00:00+09:00.",
    )
    parser.add_argument(
        "--now",
        help="Optional ISO-8601 override for current time.",
    )
    parser.add_argument(
        "--buffer-minutes",
        type=int,
        default=8,
        help="Extra buffer added before the target arrival time.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized commute payload before A2UI generation.",
    )
    parser.add_argument(
        "--message",
        help="Optional override for loading, empty, or error detail text.",
    )
    return parser.parse_args()


def emit_status(args: argparse.Namespace) -> int:
    if args.state == "loading":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.loading_title,
                detail=args.message or SPEC.loading_detail,
                hint=SPEC.loading_hint,
            )
        )
        return 0
    if args.state == "empty":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.empty_title,
                detail=args.message or SPEC.empty_detail,
                hint=SPEC.empty_hint,
            )
        )
        return 0
    if args.state == "error":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.error_title,
                detail=args.message or SPEC.error_detail,
                hint=SPEC.error_hint,
                button_label="다시 시도",
                button_event=SPEC.retry_event,
            )
        )
        return 0
    return -1


def parse_kst(value: str | None, *, fallback: datetime | None = None) -> datetime:
    if not value:
        return fallback or datetime.now(KST)
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        return dt.replace(tzinfo=KST)
    return dt.astimezone(KST)


def format_hhmm(dt: datetime) -> str:
    dt = dt.astimezone(KST)
    return f"{dt.hour:02d}:{dt.minute:02d}"


def format_duration(minutes: float) -> str:
    total = max(0, int(round(minutes)))
    hours, mins = divmod(total, 60)
    if hours and mins:
        return f"{hours}시간 {mins}분"
    if hours:
        return f"{hours}시간"
    return f"{mins}분"


def format_distance(meters: float) -> str:
    if meters >= 1000:
        return f"{meters / 1000:.1f}km"
    return f"{int(round(meters))}m"


def normalize_display_label(raw_query: str, display_name: str | None, fallback: str) -> str:
    if raw_query and len(raw_query) <= 18 and not any(ch.isdigit() for ch in raw_query):
        return clean_text(raw_query, max_len=18)
    if display_name:
        parts = [clean_text(part, max_len=16) for part in display_name.split(",")]
        filtered = [part for part in parts if part and not any(ch.isdigit() for ch in part)]
        if filtered:
            return clean_text(" ".join(filtered[:2]), max_len=18)
    return clean_text(fallback, max_len=18)


def geocode_place(query: str) -> dict[str, Any]:
    url = "https://nominatim.openstreetmap.org/search?" + urlencode(
        {
            "q": query,
            "format": "jsonv2",
            "limit": 1,
            "accept-language": "ko-KR",
        }
    )
    raw = json.loads(fetch_text(url))
    if not isinstance(raw, list) or not raw:
        raise RuntimeError(f"'{query}' 위치를 찾지 못했습니다.")
    place = raw[0]
    if not isinstance(place, dict):
        raise RuntimeError(f"'{query}' 위치 응답 형식이 올바르지 않습니다.")
    try:
        lat = float(place["lat"])
        lon = float(place["lon"])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f"'{query}' 위치 좌표를 읽지 못했습니다.") from exc
    return {
        "lat": lat,
        "lon": lon,
        "display_name": clean_text(place.get("display_name"), max_len=80),
    }


def fetch_osrm_routes(
    *,
    origin: tuple[float, float],
    destination: tuple[float, float],
    profile: str,
) -> list[dict[str, Any]]:
    coordinates = f"{origin[1]},{origin[0]};{destination[1]},{destination[0]}"
    url = (
        f"https://router.project-osrm.org/route/v1/{quote(profile)}/{coordinates}?"
        + urlencode(
            {
                "overview": "false",
                "steps": "false",
                "alternatives": "true",
                "annotations": "false",
            }
        )
    )
    payload = json.loads(fetch_text(url))
    if not isinstance(payload, dict):
        raise RuntimeError("OSRM 응답 형식이 올바르지 않습니다.")
    routes = payload.get("routes")
    if not isinstance(routes, list) or not routes:
        raise RuntimeError("경로를 찾지 못했습니다.")
    typed_routes = [route for route in routes if isinstance(route, dict)]
    if not typed_routes:
        raise RuntimeError("경로 응답에 유효한 route가 없습니다.")
    return typed_routes


def route_minutes(route: dict[str, Any]) -> float:
    try:
        return float(route.get("duration", 0.0)) / 60.0
    except (TypeError, ValueError):
        return 0.0


def route_distance(route: dict[str, Any]) -> float:
    try:
        return float(route.get("distance", 0.0))
    except (TypeError, ValueError):
        return 0.0


def commute_risk(primary_minutes: float, alternate_minutes: float | None) -> tuple[str, str]:
    if primary_minutes >= 75:
        return "높음", "장거리 이동"
    if alternate_minutes is not None:
        gap = abs(alternate_minutes - primary_minutes)
        if gap >= 15:
            return "높음", "대안 경로 편차 큼"
        if gap >= 7:
            return "보통", "대안 경로 차이 있음"
    if primary_minutes >= 45:
        return "보통", "도시권 혼잡 구간 가능"
    return "낮음", "기본 경로 기준"


def departure_detail(now: datetime, leave_by: datetime) -> str:
    delta_minutes = int(round((leave_by - now).total_seconds() / 60))
    if delta_minutes <= 0:
        overdue = abs(delta_minutes)
        if overdue <= 1:
            return "지금 출발"
        return f"{overdue}분 지연 상태"
    if delta_minutes >= 180:
        return f"약 {format_duration(delta_minutes)} 후"
    return f"지금부터 {delta_minutes}분 후"


def default_arrive_by(now: datetime) -> datetime:
    base = now.astimezone(KST).replace(second=0, microsecond=0)
    rounded = base + timedelta(minutes=(30 - (base.minute % 30)) % 30)
    if rounded <= base + timedelta(minutes=20):
        rounded += timedelta(minutes=30)
    return rounded + timedelta(minutes=30)


def normalize_osrm_commute(
    *,
    origin_query: str,
    destination_query: str,
    origin_label: str | None,
    destination_label: str | None,
    profile: str,
    arrive_by: str | None,
    now_value: str | None,
    buffer_minutes: int,
) -> dict[str, Any]:
    now = parse_kst(now_value)
    target_arrival = parse_kst(arrive_by, fallback=default_arrive_by(now))

    origin = geocode_place(origin_query)
    destination = geocode_place(destination_query)
    routes = fetch_osrm_routes(
        origin=(origin["lat"], origin["lon"]),
        destination=(destination["lat"], destination["lon"]),
        profile=profile,
    )

    primary = routes[0]
    alternate = routes[1] if len(routes) > 1 else None
    primary_minutes = route_minutes(primary)
    primary_distance = route_distance(primary)
    alternate_minutes = route_minutes(alternate) if alternate else None
    alternate_distance = route_distance(alternate) if alternate else None
    leave_by = target_arrival - timedelta(minutes=max(0, buffer_minutes) + primary_minutes)

    origin_tv = clean_text(
        origin_label or normalize_display_label(origin_query, origin.get("display_name"), "출발지"),
        max_len=18,
    )
    destination_tv = clean_text(
        destination_label
        or normalize_display_label(destination_query, destination.get("display_name"), "목적지"),
        max_len=18,
    )

    risk_level, risk_detail = commute_risk(primary_minutes, alternate_minutes)
    profile_label = "차량" if profile == "driving" else "도보"
    headline = (
        f"{origin_tv}에서 {destination_tv}까지 {format_duration(primary_minutes)} 예상, "
        f"{format_hhmm(leave_by)} 출발 권장입니다."
    )

    sections = [
        {
            "title": "주요 경로",
            "items": [
                {
                    "icon": "directionsCar" if profile == "driving" else "directionsWalk",
                    "label": profile_label,
                    "value": format_duration(primary_minutes),
                    "detail": clean_text(
                        f"{format_distance(primary_distance)} · {destination_tv} 방향",
                        max_len=60,
                    ),
                }
            ],
        }
    ]

    if alternate and alternate_minutes is not None and alternate_distance is not None:
        diff = int(round(alternate_minutes - primary_minutes))
        if diff > 0:
            diff_text = f"기본 경로보다 {diff}분 더 김"
        elif diff < 0:
            diff_text = f"기본 경로보다 {abs(diff)}분 더 짧음"
        else:
            diff_text = "기본 경로와 비슷함"
        sections.append(
            {
                "title": "대안",
                "items": [
                    {
                        "icon": "altRoute",
                        "label": "대체 경로",
                        "value": format_duration(alternate_minutes),
                        "detail": clean_text(
                            f"{format_distance(alternate_distance)} · {diff_text}",
                            max_len=60,
                        ),
                    }
                ],
            }
        )

    sections.append(
        {
            "title": "도착 목표",
            "items": [
                {
                    "icon": "event",
                    "label": "도착 목표",
                    "value": format_korean_datetime(target_arrival),
                    "detail": clean_text(
                        f"{format_hhmm(leave_by)} 출발 · 버퍼 {max(0, buffer_minutes)}분",
                        max_len=60,
                    ),
                }
            ],
        }
    )

    payload = {
        "title": "출근길 브리핑",
        "headline": clean_text(headline, max_len=88),
        "primaryMetrics": [
            {
                "label": "추천 출발",
                "value": format_hhmm(leave_by),
                "detail": departure_detail(now, leave_by),
            },
            {
                "label": "예상 소요",
                "value": format_duration(primary_minutes),
                "detail": format_distance(primary_distance),
            },
            {
                "label": "교통 리스크",
                "value": risk_level,
                "detail": clean_text(risk_detail, max_len=28),
            },
        ],
        "sections": sections,
        "alert": {
            "icon": "traffic",
            "title": "실시간 교통 미반영",
            "summary": "현재 live adapter는 Nominatim 지오코딩과 OSRM 기본 경로를 사용합니다. 실시간 교통 정체나 대중교통 지연은 별도 피드가 필요합니다.",
            "meta": "OSRM · OpenStreetMap",
        },
        "actions": [
            {"label": "새로고침", "event": "refreshCommute"},
            {"label": "대안 보기", "event": "showAlternateRoute"},
        ],
        "footer": "공용 TV에서는 출발지와 목적지의 정확한 주소 대신 요약된 위치 표현을 유지하는 편이 안전합니다.",
    }
    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "osrm":
            payload = normalize_osrm_commute(
                origin_query=args.origin,
                destination_query=args.destination,
                origin_label=args.origin_label,
                destination_label=args.destination_label,
                profile=args.profile,
                arrive_by=args.arrive_by,
                now_value=args.now,
                buffer_minutes=args.buffer_minutes,
            )
        else:
            payload = load_payload(args.input)
    except Exception as exc:
        message = clean_text(str(exc), max_len=88)
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.error_title,
                detail=message or SPEC.error_detail,
                hint=SPEC.error_hint,
                button_label="다시 시도",
                button_event=SPEC.retry_event,
            )
        )
        return 0

    if args.dump_normalized:
        args.dump_normalized.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    emit(
        build_scenario_messages(
            surface_id=args.surface_id,
            catalog_id=args.catalog_id,
            spec=SPEC,
            payload=payload,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
