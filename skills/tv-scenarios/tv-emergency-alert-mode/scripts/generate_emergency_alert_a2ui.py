#!/usr/bin/env python3
from __future__ import annotations

import argparse
import html
import json
import re
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any
from urllib.parse import urlencode

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


SPECIAL_REPORT_URL = "https://www.weather.go.kr/w/special-report/list.do"
EARTHQUAKE_URL = "https://www.weather.go.kr/w/earthquake-volcano/search/korea.do"

SPECIAL_KIND_PRIORITY = {
    "met": 0,
    "pwn": 1,
    "ann": 2,
    "inf": 3,
}

SPECIAL_KIND_LABEL = {
    "met": "특보",
    "pwn": "예비특보",
    "ann": "속보",
    "inf": "기상정보",
}

SPECIAL_LEVEL_LABEL = {
    "met": "경보",
    "pwn": "주의",
    "ann": "속보",
    "inf": "안내",
}

SPEC = ScenarioSpec(
    skill_name="tv-emergency-alert-mode",
    title="TV Emergency Alert Mode",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="emergency_main",
    loading_title="긴급 알림 준비 중",
    loading_detail="공식 소스 기반 경보 정보를 확인하고 있습니다.",
    loading_hint="영향 지역과 즉시 행동부터 먼저 채웁니다.",
    empty_title="현재 경보 없음",
    empty_detail="표시할 긴급 경보가 없습니다.",
    empty_hint="무경보 상태에서도 마지막 확인 시각을 같이 보여주는 편이 좋습니다.",
    error_title="긴급 알림을 불러오지 못했습니다",
    error_detail="공식 경보 소스 연결 또는 fail-safe 상태를 확인해 주세요.",
    error_hint="불확실한 경보는 만들어내지 말고, 소스 상태를 명확히 노출하는 편이 안전합니다.",
    retry_event="refreshEmergencyAlert",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV emergency alert skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "kma-special-report", "kma-earthquake", "kma-combined"],
        default="mock",
        help="Choose whether to use the bundled mock file or fetch live data from KMA pages.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SPEC.default_input,
        help="Path to a normalized emergency JSON file used for --source mock.",
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
        "--message",
        help="Optional override for loading, empty, or error detail text.",
    )
    parser.add_argument(
        "--min-magnitude",
        type=float,
        default=3.0,
        help="Minimum magnitude used when falling back to recent earthquake events.",
    )
    parser.add_argument(
        "--max-age-days",
        type=int,
        default=7,
        help="Maximum age in days for earthquake fallback candidates.",
    )
    parser.add_argument(
        "--now",
        help="Optional ISO-8601 override for the current time in Asia/Seoul.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized emergency payload before A2UI generation.",
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


def parse_now(value: str | None) -> datetime:
    if not value:
        return datetime.now(KST)
    parsed = datetime.fromisoformat(value)
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=KST)
    return parsed.astimezone(KST)


def strip_tags(value: str) -> str:
    normalized = re.sub(r"(?i)<br\s*/?>", "\n", value)
    normalized = re.sub(r"(?i)</(p|li|div|h4|strong|figcaption|em)>", "\n", normalized)
    normalized = re.sub(r"<[^>]+>", " ", normalized)
    return html.unescape(normalized)


def split_lines(value: str, *, max_items: int) -> list[str]:
    parts: list[str] = []
    normalized = strip_tags(value)
    normalized = normalized.replace("○", "\n○ ").replace("□", "\n□ ")
    for raw_line in normalized.splitlines():
        line = clean_text(raw_line.replace("\xa0", " ").strip(), max_len=120)
        if not line:
            continue
        if line in {"□ 내용", "<중점 사항>", "<강수 현황 및 전망>"}:
            continue
        if line.startswith("<") and line.endswith(">"):
            continue
        line = re.sub(r"^[○□\-]\s*", "", line)
        if not line:
            continue
        parts.append(line)
        if len(parts) >= max_items:
            break
    return parts


def parse_kma_datetime(value: str) -> datetime | None:
    match = re.search(
        r"(\d{4})년\s*(\d{2})월\s*(\d{2})일\s*(\d{2})시\s*(\d{2})분",
        value,
    )
    if not match:
        return None
    year, month, day, hour, minute = (int(part) for part in match.groups())
    return datetime(year, month, day, hour, minute, tzinfo=KST)


def format_hhmm(value: datetime | None) -> str:
    if value is None:
        return "-"
    localized = value.astimezone(KST)
    return f"{localized.hour:02d}:{localized.minute:02d}"


def format_relative_days(now: datetime, target: datetime | None) -> str:
    if target is None:
        return "발표 시각 확인"
    delta = now - target.astimezone(KST)
    days = delta.days
    if days <= 0:
        hours = max(0, int(delta.total_seconds() // 3600))
        if hours <= 0:
            minutes = max(0, int(delta.total_seconds() // 60))
            return f"{minutes}분 전"
        return f"{hours}시간 전"
    return f"{days}일 전"


def first_match(pattern: str, value: str) -> str:
    match = re.search(pattern, value, re.S)
    if not match:
        return ""
    return match.group(1)


def fetch_special_report_options() -> list[dict[str, str]]:
    page = fetch_text(SPECIAL_REPORT_URL)
    select_block = first_match(
        r'<select id="select-list" name="reportId">(.*?)</select>',
        page,
    )
    if not select_block:
        return []
    options: list[dict[str, str]] = []
    for raw_value, raw_label in re.findall(
        r'<option value="([^"]*)"[^>]*>(.*?)</option>',
        select_block,
        re.S,
    ):
        value = clean_text(raw_value, max_len=80)
        if ":" not in value:
            continue
        kind = value.split(":", 1)[0]
        label = clean_text(strip_tags(raw_label), max_len=88)
        options.append({"value": value, "kind": kind, "label": label})
    return options


def select_special_report(options: list[dict[str, str]]) -> dict[str, str] | None:
    candidates = [
        option
        for option in options
        if option.get("kind") in SPECIAL_KIND_PRIORITY
    ]
    if not candidates:
        return None
    candidates.sort(key=lambda option: SPECIAL_KIND_PRIORITY[option["kind"]])
    return candidates[0]


def fetch_special_report_payload(report: dict[str, str], now: datetime) -> dict[str, Any]:
    kind = report["kind"]
    report_id = report["value"]
    detail_url = f"{SPECIAL_REPORT_URL}?{urlencode({'kind': kind, 'reportId': report_id})}"
    page = fetch_text(detail_url)

    announce_html = first_match(r'<div class="cmp-view-announce">(.*?)</div>', page)
    header_html = first_match(r'<div class="cmp-view-header">\s*<h4>(.*?)</h4>', page)
    content_html = first_match(r'<div class="cmp-view-content">(.*?)</section>', page)
    if not announce_html or not header_html or not content_html:
        raise RuntimeError("기상청 특보 페이지 구조를 읽지 못했습니다.")

    announce_text = clean_text(strip_tags(announce_html), max_len=140)
    announce_parts = [part.strip() for part in announce_text.split("|") if part.strip()]
    area = announce_parts[0] if announce_parts else "전국"
    published_text = announce_parts[1] if len(announce_parts) >= 2 else ""
    published_at = parse_kma_datetime(published_text)
    announce_meta = announce_parts[2] if len(announce_parts) >= 3 else ""

    header_text = clean_text(strip_tags(header_html), max_len=160)
    header_parts = [part.strip() for part in header_text.split("|") if part.strip()]
    report_number = header_parts[0] if header_parts else report_id
    report_type = header_parts[1] if len(header_parts) >= 2 else SPECIAL_KIND_LABEL.get(kind, "공식 알림")
    subject = header_parts[2] if len(header_parts) >= 3 else report.get("label", report_type)

    summary_lines = split_lines(content_html, max_items=3)
    summary = clean_text(" ".join(summary_lines[:2]), max_len=100)
    figure_alt = clean_text(first_match(r'<img [^>]*alt="([^"]*)"', content_html), max_len=32)
    figure_caption = clean_text(strip_tags(first_match(r"<figcaption>(.*?)</figcaption>", content_html)), max_len=40)

    level = SPECIAL_LEVEL_LABEL.get(kind, "안내")
    title = "긴급 알림 모드" if kind in {"met", "pwn", "ann"} else "공식 주의 정보"
    headline = clean_text(subject or report.get("label"), max_len=88)

    sections = [
        {
            "title": "핵심 안내",
            "items": [
                {
                    "icon": "warning" if kind in {"met", "pwn", "ann"} else "info",
                    "label": report_type,
                    "value": clean_text(summary_lines[0] if summary_lines else subject, max_len=32),
                    "detail": clean_text(summary_lines[1] if len(summary_lines) >= 2 else summary, max_len=72),
                }
            ],
        },
        {
            "title": "공식 확인",
            "items": [
                {
                    "icon": "place",
                    "label": "영향 지역",
                    "value": clean_text(area, max_len=28),
                    "detail": clean_text(announce_meta or published_text, max_len=72),
                }
            ],
        },
    ]

    if figure_alt or figure_caption:
        sections[1]["items"].append(
            {
                "icon": "image",
                "label": "자료 유형",
                "value": figure_alt or "첨부 자료",
                "detail": figure_caption or "공식 도표 포함",
            }
        )

    return {
        "title": title,
        "headline": headline,
        "primaryMetrics": [
            {
                "label": "알림 수준",
                "value": level,
                "detail": clean_text(report_type, max_len=24),
            },
            {
                "label": "영향 지역",
                "value": clean_text(area, max_len=28),
                "detail": clean_text(report_number, max_len=24),
            },
            {
                "label": "발표 시각",
                "value": format_hhmm(published_at),
                "detail": format_relative_days(now, published_at),
            },
        ],
        "sections": sections,
        "alert": {
            "icon": "report",
            "title": clean_text(report_type, max_len=28),
            "summary": summary or clean_text(subject, max_len=100),
            "meta": clean_text("기상청 날씨누리", max_len=40),
        },
        "actions": [
            {"label": "새로고침", "event": "refreshEmergencyAlert"},
            {"label": "상세 확인", "event": "openEmergencyDetail"},
        ],
        "footer": "공식 특보와 기상정보를 기준으로 TV용 요약만 제공합니다. 실제 행동 판단은 기상청 원문과 재난문자를 우선하세요.",
    }


def parse_earthquake_events(page: str) -> list[dict[str, Any]]:
    tbody = first_match(
        r'<table class="table-col eqk-search-table whitespaced" id="excel_body">.*?<tbody>(.*?)</tbody>',
        page,
    )
    if not tbody:
        return []

    events: list[dict[str, Any]] = []
    for row_html in re.findall(r"<tr>(.*?)</tr>", tbody, re.S):
        cells = [
            clean_text(strip_tags(cell), max_len=120)
            for cell in re.findall(r"<td[^>]*>(.*?)</td>", row_html, re.S)
        ]
        if len(cells) < 10:
            continue
        occurred_at = None
        try:
            occurred_at = datetime.strptime(cells[1], "%Y/%m/%d %H:%M:%S").replace(tzinfo=KST)
            magnitude = float(cells[2])
            depth_km = int(float(cells[3]))
        except (ValueError, TypeError):
            continue

        detail_url = first_match(r'<a href="([^"]+)"[^>]*><span>상세정보</span>', row_html)
        events.append(
            {
                "occurred_at": occurred_at,
                "magnitude": magnitude,
                "depth_km": depth_km,
                "max_intensity": cells[4],
                "location": clean_text(cells[7], max_len=40),
                "detail_url": clean_text(detail_url, max_len=120),
            }
        )
    return events


def earthquake_level(event: dict[str, Any]) -> str:
    magnitude = float(event.get("magnitude", 0.0))
    if magnitude >= 4.5:
        return "경계"
    if magnitude >= 3.5:
        return "주의"
    return "관찰"


def fetch_earthquake_payload(args: argparse.Namespace, now: datetime) -> dict[str, Any] | None:
    page = fetch_text(EARTHQUAKE_URL)
    events = parse_earthquake_events(page)
    if not events:
        return None

    max_age = timedelta(days=max(1, args.max_age_days))
    candidates = [
        event
        for event in events
        if float(event["magnitude"]) >= args.min_magnitude
        and now - event["occurred_at"] <= max_age
    ]
    if not candidates:
        return None

    event = candidates[0]
    level = earthquake_level(event)
    occurred_at = event["occurred_at"]
    headline = clean_text(
        f"{event['location']} 규모 {event['magnitude']:.1f} 지진이 관측되었습니다.",
        max_len=88,
    )
    detail_meta = f"최대진도 {event['max_intensity']} · 깊이 {event['depth_km']}km"

    return {
        "title": "긴급 알림 모드",
        "headline": headline,
        "primaryMetrics": [
            {
                "label": "지진 규모",
                "value": f"{event['magnitude']:.1f}",
                "detail": clean_text(level, max_len=20),
            },
            {
                "label": "최대 진도",
                "value": clean_text(event["max_intensity"], max_len=12),
                "detail": clean_text(event["location"], max_len=28),
            },
            {
                "label": "발생 시각",
                "value": format_hhmm(occurred_at),
                "detail": format_relative_days(now, occurred_at),
            },
        ],
        "sections": [
            {
                "title": "발생 정보",
                "items": [
                    {
                        "icon": "warning",
                        "label": "발생 위치",
                        "value": clean_text(event["location"], max_len=32),
                        "detail": clean_text(detail_meta, max_len=72),
                    },
                    {
                        "icon": "schedule",
                        "label": "공식 발표",
                        "value": format_korean_datetime(occurred_at),
                        "detail": "기상청 국내지진조회 기준",
                    },
                ],
            },
            {
                "title": "즉시 확인",
                "items": [
                    {
                        "icon": "warning",
                        "label": "추가 안내",
                        "value": "실내 낙하물 주의",
                        "detail": "여진 가능성과 시설물 상태를 공식 안내로 다시 확인하세요.",
                    }
                ],
            },
        ],
        "alert": {
            "icon": "report",
            "title": "최근 유의 지진",
            "summary": clean_text(
                f"{event['location']}에서 규모 {event['magnitude']:.1f}, 최대진도 {event['max_intensity']}가 기록되었습니다.",
                max_len=100,
            ),
            "meta": clean_text("기상청 지진·화산", max_len=40),
        },
        "actions": [
            {"label": "새로고침", "event": "refreshEmergencyAlert"},
            {"label": "상세 확인", "event": "openEmergencyDetail"},
        ],
        "footer": "지진 정보는 기상청 공개 목록을 기준으로 요약했습니다. 대피 판단은 재난문자와 공식 행동요령을 우선하세요.",
    }


def fetch_live_payload(args: argparse.Namespace) -> tuple[dict[str, Any] | None, str]:
    now = parse_now(args.now)

    if args.source == "kma-special-report":
        report = select_special_report(fetch_special_report_options())
        if report is None:
            return None, "현재 표시할 공식 특보나 기상정보가 없습니다."
        return fetch_special_report_payload(report, now), ""

    if args.source == "kma-earthquake":
        payload = fetch_earthquake_payload(args, now)
        return payload, "최근 유의 지진이 없습니다."

    options = fetch_special_report_options()
    report = select_special_report(options)
    if report is not None:
        return fetch_special_report_payload(report, now), ""

    payload = fetch_earthquake_payload(args, now)
    if payload is not None:
        return payload, ""
    return None, "현재 공식 특보와 최근 유의 지진이 없습니다."


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "mock":
            payload = load_payload(args.input)
            empty_reason = ""
        else:
            payload, empty_reason = fetch_live_payload(args)
            if payload is None:
                if args.dump_normalized:
                    args.dump_normalized.write_text(
                        json.dumps(
                            {"status": "empty", "detail": empty_reason or SPEC.empty_detail},
                            ensure_ascii=False,
                            indent=2,
                        )
                        + "\n",
                        encoding="utf-8",
                    )
                emit(
                    build_status_messages(
                        surface_id=args.surface_id,
                        catalog_id=args.catalog_id,
                        title=SPEC.empty_title,
                        detail=empty_reason or SPEC.empty_detail,
                        hint=SPEC.empty_hint,
                    )
                )
                return 0
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
