#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timedelta
from html import unescape
from pathlib import Path
from typing import Any
from urllib.parse import urlencode

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import CONNECT_TIMEOUT_SECONDS, KST, MAX_FETCH_SECONDS, fetch_text
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


SPEC = ScenarioSpec(
    skill_name="tv-travel-assistant",
    title="TV Travel Assistant",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="travel_main",
    loading_title="공항 출발 정보를 준비 중",
    loading_detail="운항편, 게이트, 공항 혼잡도를 공식 소스 기준으로 모으고 있습니다.",
    loading_hint="가장 가까운 출발편과 체크포인트부터 먼저 채웁니다.",
    empty_title="표시할 출발편 없음",
    empty_detail="현재 조건에 맞는 공항 출발 정보가 없습니다.",
    empty_hint="항공편 번호, 날짜, 터미널, 시간 창을 다시 확인해 주세요.",
    error_title="여행 정보를 불러오지 못했습니다",
    error_detail="인천공항 출발편 또는 혼잡도 소스 연결 상태를 확인해 주세요.",
    error_hint="예약번호 대신 운항편명, 터미널, 게이트 중심으로 요약하는 편이 TV에 안전합니다.",
    retry_event="refreshTravel",
)


AIRPORT_SITE_ID = "ap_ko"
AIRPORT_LANG = "ko"
DEPARTURE_LIST_URL = "https://airport.kr/dep/ap_ko/getDepPasSchList.do"
DEPARTURE_DETAIL_URL = "https://airport.kr/dep/ap_ko/depPasSchDetail.do"
CONGESTION_URL = "https://www.airport.kr/ap_ko/883/subview.do"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV travel assistant skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "airport-kr"],
        default="mock",
        help="Choose whether to use the bundled mock file or fetch live departure data from airport.kr.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SPEC.default_input,
        help="Path to a normalized travel JSON file used for --source mock.",
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
        "--now",
        help="Optional ISO-8601 override for the current time in Asia/Seoul.",
    )
    parser.add_argument(
        "--date",
        help="Optional departure date in YYYYMMDD. Defaults to the current date in Asia/Seoul.",
    )
    parser.add_argument(
        "--window-hours",
        type=int,
        default=4,
        help="How many hours ahead to search when explicit time filters are not provided.",
    )
    parser.add_argument(
        "--from-time",
        help="Optional search start in HHMM.",
    )
    parser.add_argument(
        "--to-time",
        help="Optional search end in HHMM.",
    )
    parser.add_argument(
        "--flight-number",
        help="Optional exact flight number filter, e.g. KE913.",
    )
    parser.add_argument(
        "--destination-code",
        help="Optional destination airport code filter, e.g. NRT or KIX.",
    )
    parser.add_argument(
        "--terminal",
        choices=["T1", "T2"],
        help="Optional terminal filter.",
    )
    parser.add_argument(
        "--airline",
        help="Optional airline code filter used by the airport list endpoint.",
    )
    parser.add_argument(
        "--include-codeshare",
        action="store_true",
        help="Include codeshare slave rows when selecting a flight.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized travel payload before A2UI generation.",
    )
    return parser.parse_args()


def emit_status(args: argparse.Namespace, state: str, detail: str | None = None) -> int:
    if state == "loading":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.loading_title,
                detail=detail or args.message or SPEC.loading_detail,
                hint=SPEC.loading_hint,
            )
        )
        return 0
    if state == "empty":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.empty_title,
                detail=detail or args.message or SPEC.empty_detail,
                hint=SPEC.empty_hint,
            )
        )
        return 0
    if state == "error":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=SPEC.error_title,
                detail=detail or args.message or SPEC.error_detail,
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
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        return dt.replace(tzinfo=KST)
    return dt.astimezone(KST)


def parse_date_token(value: str | None, *, fallback: datetime) -> str:
    if not value:
        return fallback.strftime("%Y%m%d")
    stripped = value.strip()
    if re.fullmatch(r"\d{8}", stripped):
        return stripped
    try:
        dt = datetime.fromisoformat(stripped)
    except ValueError as exc:
        raise RuntimeError("date must be YYYYMMDD or ISO-8601.") from exc
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=KST)
    return dt.astimezone(KST).strftime("%Y%m%d")


def normalize_hhmm(value: str) -> str:
    stripped = value.strip().replace(":", "")
    if not re.fullmatch(r"\d{4}", stripped):
        raise RuntimeError("time values must be HHMM or HH:MM.")
    hour = int(stripped[:2])
    minute = int(stripped[2:])
    if hour > 23 or minute > 59:
        raise RuntimeError("time values must be valid 24-hour timestamps.")
    return stripped


def derive_time_window(
    *,
    now: datetime,
    from_time: str | None,
    to_time: str | None,
    window_hours: int,
    prefer_full_day: bool,
) -> tuple[str, str]:
    if from_time and to_time:
        return normalize_hhmm(from_time), normalize_hhmm(to_time)
    if prefer_full_day and not from_time and not to_time:
        return "0000", "2359"
    base = now.astimezone(KST).replace(second=0, microsecond=0)
    derived_from = base.strftime("%H%M")
    derived_to_dt = base + timedelta(hours=max(1, window_hours))
    derived_to = derived_to_dt.strftime("%H%M")
    if from_time:
        return normalize_hhmm(from_time), derived_to
    if to_time:
        return derived_from, normalize_hhmm(to_time)
    return derived_from, derived_to


def airport_post_json(url: str, form: dict[str, str]) -> dict[str, Any]:
    try:
        response = subprocess.run(
            [
                "curl",
                "-L",
                "--fail",
                "--silent",
                "--show-error",
                "--connect-timeout",
                str(CONNECT_TIMEOUT_SECONDS),
                "--max-time",
                str(MAX_FETCH_SECONDS),
                "-X",
                "POST",
                "-H",
                "Accept: application/json",
                "-d",
                urlencode(form),
                url,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("curl is required for live airport fetches.") from exc
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(exc.stderr.strip() or "Airport live fetch failed.") from exc
    payload = json.loads(response.stdout)
    if not isinstance(payload, dict):
        raise RuntimeError("Airport live response must be a JSON object.")
    return payload


def parse_departure_timestamp(value: str | None, *, fallback_date: str) -> datetime | None:
    if not value:
        return None
    stripped = value.strip()
    for fmt in ("%Y%m%d%H%M", "%Y.%m.%d %H:%M"):
        try:
            return datetime.strptime(stripped, fmt).replace(tzinfo=KST)
        except ValueError:
            continue
    if re.fullmatch(r"\d{2}:\d{2}", stripped):
        return datetime.strptime(f"{fallback_date}{stripped.replace(':', '')}", "%Y%m%d%H%M").replace(
            tzinfo=KST
        )
    return None


def strip_tags(fragment: str) -> str:
    return clean_text(unescape(re.sub(r"<[^>]+>", " ", fragment)), max_len=120)


def fetch_departure_schedule(
    *,
    date: str,
    from_hhmm: str,
    to_hhmm: str,
    now: datetime,
    terminal: str | None,
    airline: str | None,
    flight_number: str | None,
    destination_code: str | None,
) -> list[dict[str, Any]]:
    form = {
        "siteId": AIRPORT_SITE_ID,
        "langSe": AIRPORT_LANG,
        "daySel": date,
        "todayDate": now.strftime("%Y%m%d"),
        "tomorrowDate": (now + timedelta(days=1)).strftime("%Y%m%d"),
        "todayTime": now.strftime("%H%M"),
        "curDate": date,
        "curStime": from_hhmm,
        "curEtime": to_hhmm,
        "fromTime": from_hhmm,
        "toTime": to_hhmm,
        "page": "1",
        "row": "100",
        "arrOrDep": "D",
        "porc": "P",
        "intg": "",
        "keyWord": "",
    }
    if terminal:
        form["termId"] = terminal

    payload = airport_post_json(DEPARTURE_LIST_URL, form)
    schedules = payload.get("scheduleList")
    if not isinstance(schedules, list):
        raise RuntimeError("출발편 목록 응답 형식이 올바르지 않습니다.")
    return [item for item in schedules if isinstance(item, dict)]


def is_codeshare_slave(schedule: dict[str, Any]) -> bool:
    return clean_text(schedule.get("codeshare"), max_len=16).lower() == "slave"


def schedule_matches(
    schedule: dict[str, Any],
    *,
    flight_number: str | None,
    destination_code: str | None,
    terminal: str | None,
    airline: str | None,
    include_codeshare: bool,
) -> bool:
    if terminal and clean_text(schedule.get("terminal"), max_len=4) != terminal:
        return False
    if destination_code and clean_text(schedule.get("p1code"), max_len=8).upper() != destination_code.upper():
        return False
    if airline and clean_text(schedule.get("flightCarrier"), max_len=8).upper() != airline.upper():
        return False
    if flight_number:
        candidates = {
            clean_text(schedule.get("fnumber"), max_len=16).upper(),
            clean_text(schedule.get("masterflight"), max_len=16).upper(),
            clean_text(schedule.get("codeshareFlight"), max_len=16).upper(),
        }
        if flight_number.upper() not in candidates:
            return False
    if not include_codeshare and is_codeshare_slave(schedule) and not flight_number:
        return False
    return True


def schedule_priority(schedule: dict[str, Any], *, now: datetime, date: str) -> tuple[int, datetime, str]:
    status = clean_text(schedule.get("stattxt"), max_len=16)
    departure = parse_departure_timestamp(schedule.get("etime"), fallback_date=date)
    actual = parse_departure_timestamp(schedule.get("btime"), fallback_date=date)
    effective = actual or departure or now

    if status == "출발":
        bucket = 4
    elif status == "탑승마감":
        bucket = 3
    elif effective < now - timedelta(minutes=20):
        bucket = 2
    elif effective < now:
        bucket = 1
    else:
        bucket = 0
    return bucket, effective, clean_text(schedule.get("fnumber"), max_len=16)


def flight_match_rank(schedule: dict[str, Any], *, flight_number: str | None) -> int:
    if not flight_number:
        return 0
    target = flight_number.upper()
    if clean_text(schedule.get("fnumber"), max_len=16).upper() == target:
        return 0
    if clean_text(schedule.get("masterflight"), max_len=16).upper() == target:
        return 1
    if clean_text(schedule.get("codeshareFlight"), max_len=16).upper() == target:
        return 2
    return 3


def choose_schedule(
    schedules: list[dict[str, Any]],
    *,
    now: datetime,
    date: str,
    flight_number: str | None,
    destination_code: str | None,
    terminal: str | None,
    airline: str | None,
    include_codeshare: bool,
) -> dict[str, Any] | None:
    filtered = [
        schedule
        for schedule in schedules
        if schedule_matches(
            schedule,
            flight_number=flight_number,
            destination_code=destination_code,
            terminal=terminal,
            airline=airline,
            include_codeshare=include_codeshare,
        )
    ]
    if not filtered:
        return None
    filtered.sort(
        key=lambda item: (
            flight_match_rank(item, flight_number=flight_number),
            schedule_priority(item, now=now, date=date),
        )
    )
    return filtered[0]


def fetch_departure_detail(schedule: dict[str, Any]) -> dict[str, Any]:
    afs_id = clean_text(schedule.get("afsId"), max_len=40)
    airport_code = clean_text(schedule.get("p1code"), max_len=8)
    if not afs_id or not airport_code:
        raise RuntimeError("선택한 운항편의 상세 조회 정보가 부족합니다.")
    payload = airport_post_json(
        DEPARTURE_DETAIL_URL,
        {
            "afsId": afs_id,
            "airportCode": airport_code,
        },
    )
    if not isinstance(payload.get("viewInfo"), dict):
        raise RuntimeError("운항편 상세 응답에 viewInfo가 없습니다.")
    return payload


def parse_congestion_rows(html: str) -> list[tuple[str, int]]:
    table_match = re.search(r'<table id="userEx".*?<tbody>(.*?)</tbody>', html, re.DOTALL)
    if table_match is None:
        raise RuntimeError("공항 혼잡도 표를 찾지 못했습니다.")

    rows: list[tuple[str, int]] = []
    for label, cells_html in re.findall(r"<tr[^>]*>\s*<th>([^<]+)</th>(.*?)</tr>", table_match.group(1), re.DOTALL):
        raw_cells = re.findall(r"<td[^>]*>(.*?)</td>", cells_html, re.DOTALL)
        cells = [strip_tags(cell) for cell in raw_cells]
        if not cells:
            continue
        totals = [cell for cell in cells if re.fullmatch(r"\d+", cell)]
        if not totals:
            continue
        rows.append((strip_tags(label), int(totals[-1])))
    return rows


def fetch_congestion_summary(*, date: str, terminal: str) -> dict[str, Any]:
    html = fetch_text(f"{CONGESTION_URL}?{urlencode({'selTm': terminal, 'pday': date})}")
    rows = parse_congestion_rows(html)
    if not rows:
        raise RuntimeError("공항 혼잡도 행을 읽지 못했습니다.")
    return {
        "rows": rows,
        "terminal": terminal,
        "date": date,
    }


def find_congestion_row(rows: list[tuple[str, int]], target_hour: int) -> tuple[str, int] | None:
    desired_prefix = f"{target_hour:02d}~{(target_hour + 1) % 24:02d}시"
    for label, total in rows:
        if label.startswith(desired_prefix):
            return label, total
    return rows[0] if rows else None


def congestion_level(total: int) -> tuple[str, str]:
    if total >= 4000:
        return "높음", "출국장 혼잡 가능"
    if total >= 2200:
        return "보통", "혼잡 대비 필요"
    return "낮음", "비교적 원활"


def format_countdown(target: datetime, *, now: datetime) -> str:
    delta_minutes = int(round((target - now).total_seconds() / 60))
    if delta_minutes <= 0:
        overdue = abs(delta_minutes)
        if overdue <= 2:
            return "지금"
        return f"{overdue}분 경과"
    hours, minutes = divmod(delta_minutes, 60)
    if hours and minutes:
        return f"{hours}시간 {minutes}분"
    if hours:
        return f"{hours}시간"
    return f"{minutes}분"


def format_hhmm(dt: datetime | None) -> str:
    if dt is None:
        return "-"
    return dt.astimezone(KST).strftime("%H:%M")


def format_clock_text(value: str | None) -> str:
    text = clean_text(value, max_len=20)
    match = re.search(r"(\d{2}:\d{2})", text)
    if match:
        return match.group(1)
    return text or "-"


def format_temperature(value: Any) -> str:
    try:
        return f"{int(round(float(value)))}°"
    except (TypeError, ValueError):
        return "-"


def format_elapsed_time(value: str | None) -> str:
    text = clean_text(value, max_len=8)
    if re.fullmatch(r"\d{4}", text):
        hour = int(text[:2])
        minute = int(text[2:])
        if hour and minute:
            return f"{hour}시간 {minute}분"
        if hour:
            return f"{hour}시간"
        return f"{minute}분"
    return ""


def detail_text(*parts: str) -> str:
    return clean_text(" · ".join(part for part in parts if part), max_len=72)


def normalize_airport_travel(
    *,
    schedule: dict[str, Any],
    detail: dict[str, Any],
    congestion: dict[str, Any] | None,
    now: datetime,
    date: str,
) -> dict[str, Any]:
    view_info = detail.get("viewInfo", {})
    city_info = detail.get("cityInfo") if isinstance(detail.get("cityInfo"), dict) else {}
    weather_list = detail.get("weatherList") if isinstance(detail.get("weatherList"), list) else []
    weather_info = weather_list[0] if weather_list and isinstance(weather_list[0], dict) else {}

    scheduled_departure = parse_departure_timestamp(schedule.get("etime"), fallback_date=date)
    actual_departure = parse_departure_timestamp(schedule.get("btime"), fallback_date=date)
    current_departure = actual_departure or scheduled_departure
    status = clean_text(schedule.get("stattxt"), max_len=16) or "운항 정보"
    terminal = clean_text(view_info.get("terminal") or schedule.get("terminal"), max_len=4) or "T1"
    gate = clean_text(view_info.get("gatenumber") or schedule.get("gatenumber"), max_len=12) or "-"
    flight_number = clean_text(schedule.get("fnumber"), max_len=16)
    destination = clean_text(schedule.get("airportName1") or view_info.get("airportNameKo"), max_len=24)
    airport_code = clean_text(schedule.get("p1code"), max_len=8)
    counter = clean_text(view_info.get("airCounter") or schedule.get("chkinrange"), max_len=20) or "-"
    local_time = format_clock_text(detail.get("timeZoneHour"))
    local_date = clean_text(detail.get("timeZoneDate"), max_len=24)
    move_time = clean_text(view_info.get("moveTimeKo"), max_len=20) or "-"
    destination_weather = clean_text(weather_info.get("weather"), max_len=16)
    destination_temp = format_temperature(weather_info.get("temp"))
    elapsed = format_elapsed_time(
        schedule.get("elapseTime") or detail.get("airportInfo", {}).get("elapseTime")
    )

    congestion_metric = {
        "label": "출국 혼잡",
        "value": "확인 필요",
        "detail": "공항 혼잡도 표 확인",
    }
    congestion_item = {
        "icon": "groups",
        "label": "출국장 혼잡",
        "value": "확인 필요",
        "detail": "공항 혼잡도 예고를 읽지 못했습니다.",
    }
    if congestion is not None:
        target_row = find_congestion_row(congestion["rows"], current_departure.hour if current_departure else now.hour)
        if target_row is not None:
            slot, total = target_row
            level, level_detail = congestion_level(total)
            congestion_metric = {
                "label": "출국 혼잡",
                "value": level,
                "detail": clean_text(f"{slot} {total}명 예고", max_len=32),
            }
            congestion_item = {
                "icon": "groups",
                "label": "출국장 혼잡",
                "value": level,
                "detail": detail_text(slot, f"{total}명 예고", level_detail),
            }

    headline = clean_text(
        f"{flight_number} {destination}행 {format_hhmm(current_departure)} 출발 기준으로 게이트와 공항 체크포인트를 정리했습니다.",
        max_len=88,
    )
    primary_metrics = [
        {
            "label": "탑승까지",
            "value": format_countdown(current_departure or now, now=now),
            "detail": detail_text(f"{terminal} · 게이트 {gate}", status),
        },
        {
            "label": "체크인",
            "value": counter,
            "detail": detail_text("출국심사 후 이동", move_time),
        },
        congestion_metric,
    ]

    sections = [
        {
            "title": "항공편",
            "items": [
                {
                    "icon": "flightTakeoff",
                    "label": flight_number,
                    "value": clean_text(f"{format_hhmm(current_departure)} 출발", max_len=24),
                    "detail": detail_text(destination, airport_code, status),
                },
                {
                    "icon": "place",
                    "label": "게이트",
                    "value": gate,
                    "detail": detail_text(terminal, f"체크인 {counter}"),
                },
            ],
        },
        {
            "title": "체크포인트",
            "items": [
                {
                    "icon": "security",
                    "label": "출국 후 이동",
                    "value": move_time,
                    "detail": detail_text("주차장-연결통로-체크인-탑승구 순서", "공항 내부 기준"),
                },
                congestion_item,
            ],
        },
        {
            "title": "목적지",
            "items": [
                {
                    "icon": "schedule",
                    "label": "현지 시각",
                    "value": local_time or "-",
                    "detail": local_date or "현지 날짜 확인 필요",
                },
                {
                    "icon": "wbSunny",
                    "label": "도착지 날씨",
                    "value": clean_text(f"{destination_weather} {destination_temp}", max_len=24),
                    "detail": detail_text(
                        f"비행 {elapsed}" if elapsed else "",
                        clean_text(city_info.get("countryNameKo"), max_len=16),
                    ),
                },
            ],
        },
    ]

    if status == "지연":
        alert = {
            "icon": "warning",
            "title": "운항 상태 변경됨",
            "summary": clean_text(
                f"{flight_number}편이 지연 상태입니다. 항공사 앱이나 공항 전광판에서 최종 출발 시각을 다시 확인해 주세요.",
                max_len=100,
            ),
            "meta": clean_text(f"{terminal} · 게이트 {gate}", max_len=32),
        }
    elif status == "탑승마감":
        alert = {
            "icon": "update",
            "title": "탑승 마감 상태",
            "summary": clean_text(
                f"{flight_number}편은 탑승마감 상태로 표시됩니다. 동행자 확인용 화면이라면 새 운항편을 다시 선택하는 편이 안전합니다.",
                max_len=100,
            ),
            "meta": clean_text(f"{terminal} · 게이트 {gate}", max_len=32),
        }
    else:
        alert = {
            "icon": "update",
            "title": "운영 정보 변동 가능",
            "summary": "게이트, 출발시각, 출국장 혼잡도는 짧은 시간에도 바뀔 수 있으니 마지막 조회 시각을 함께 유지하는 편이 안전합니다.",
            "meta": "인천공항 공식 출발편",
        }

    payload = {
        "title": "공항 출발 어시스턴트",
        "headline": headline,
        "primaryMetrics": primary_metrics,
        "sections": sections,
        "alert": alert,
        "actions": [
            {"label": "새로고침", "event": "refreshTravel"},
            {"label": "다른 운항편", "event": "showTravelFlights"},
        ],
        "footer": "live adapter는 인천공항 공식 출발편과 공항 예상 혼잡도 페이지를 사용합니다. 예약번호 대신 운항편명, 터미널, 게이트 중심으로 요약하는 편이 TV에 적합합니다.",
    }
    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args, args.state)
    if status == 0:
        return 0

    try:
        if args.source == "airport-kr":
            now = parse_now(args.now)
            date = parse_date_token(args.date, fallback=now)
            from_hhmm, to_hhmm = derive_time_window(
                now=now,
                from_time=args.from_time,
                to_time=args.to_time,
                window_hours=args.window_hours,
                prefer_full_day=bool(args.flight_number or args.destination_code or args.airline),
            )
            schedules = fetch_departure_schedule(
                date=date,
                from_hhmm=from_hhmm,
                to_hhmm=to_hhmm,
                now=now,
                terminal=args.terminal,
                airline=args.airline,
                flight_number=args.flight_number,
                destination_code=args.destination_code,
            )
            selected = choose_schedule(
                schedules,
                now=now,
                date=date,
                flight_number=args.flight_number,
                destination_code=args.destination_code,
                terminal=args.terminal,
                airline=args.airline,
                include_codeshare=args.include_codeshare,
            )
            if selected is None:
                detail = "조건에 맞는 출발편을 찾지 못했습니다."
                return emit_status(args, "empty", detail=detail)

            detail_payload = fetch_departure_detail(selected)
            terminal = clean_text(
                detail_payload.get("viewInfo", {}).get("terminal") or selected.get("terminal"),
                max_len=4,
            ) or args.terminal or "T1"
            congestion_payload: dict[str, Any] | None
            try:
                congestion_payload = fetch_congestion_summary(date=date, terminal=terminal)
            except Exception:
                congestion_payload = None
            payload = normalize_airport_travel(
                schedule=selected,
                detail=detail_payload,
                congestion=congestion_payload,
                now=now,
                date=date,
            )
        else:
            payload = load_payload(args.input)
    except Exception as exc:
        return emit_status(args, "error", detail=clean_text(str(exc), max_len=88))

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
