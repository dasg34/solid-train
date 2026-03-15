#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

from dateutil.rrule import rrulestr

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import KST, fetch_text, format_korean_date, format_korean_datetime
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


DEFAULT_ICS_URL = "https://holidays.hyunbin.page/basic.ics"

SPEC = ScenarioSpec(
    skill_name="tv-schedule-briefing",
    title="TV Schedule Briefing",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="schedule_main",
    loading_title="일정 준비 중",
    loading_detail="오늘 일정과 다음 약속을 정리하고 있습니다.",
    loading_hint="현재 일정, 다음 일정, 충돌 여부를 먼저 채웁니다.",
    empty_title="표시할 일정 없음",
    empty_detail="오늘 연결된 일정이 아직 없습니다.",
    empty_hint="캘린더 연동 또는 mock 일정을 먼저 확인해 주세요.",
    error_title="일정을 불러오지 못했습니다",
    error_detail="캘린더 연결 또는 권한 상태를 확인해 주세요.",
    error_hint="공용 화면 노출을 고려해 민감 정보 마스킹 규칙도 함께 점검하세요.",
    retry_event="refreshSchedule",
)


@dataclass
class ScheduleEvent:
    start: datetime
    end: datetime
    summary: str
    location: str
    description: str
    all_day: bool


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV schedule briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "ics-url", "ics-file"],
        default="mock",
        help="Choose whether to use the bundled mock file or a live ICS calendar feed.",
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
        "--ics-url",
        default=DEFAULT_ICS_URL,
        help="ICS URL used when --source ics-url is selected.",
    )
    parser.add_argument(
        "--ics-file",
        type=Path,
        help="Path to an ICS file used when --source ics-file is selected.",
    )
    parser.add_argument(
        "--days",
        type=int,
        default=2,
        help="Number of days ahead to include from the ICS feed.",
    )
    parser.add_argument(
        "--max-events",
        type=int,
        default=6,
        help="Maximum number of expanded events to include from the live schedule feed.",
    )
    parser.add_argument(
        "--now",
        help="Optional ISO-8601 override for the current time, e.g. 2026-03-15T08:00:00+09:00.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized schedule payload before A2UI generation.",
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


def parse_now(value: str | None) -> datetime:
    if not value:
        return datetime.now(KST)
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        return dt.replace(tzinfo=KST)
    return dt.astimezone(KST)


def unfold_ics_lines(text: str) -> list[str]:
    unfolded: list[str] = []
    for line in text.splitlines():
        if line.startswith((" ", "\t")) and unfolded:
            unfolded[-1] += line[1:]
        else:
            unfolded.append(line.rstrip())
    return unfolded


def split_content_line(line: str) -> tuple[str, dict[str, str], str]:
    head, value = line.split(":", 1)
    parts = head.split(";")
    name = parts[0]
    params: dict[str, str] = {}
    for part in parts[1:]:
        if "=" in part:
            key, param_value = part.split("=", 1)
            params[key] = param_value
    return name, params, value


def parse_ics_datetime(value: str, params: dict[str, str]) -> tuple[datetime, bool]:
    tzid = params.get("TZID")
    zone = KST
    if tzid:
        try:
            zone = ZoneInfo(tzid)
        except Exception:
            zone = KST
    if params.get("VALUE") == "DATE" or len(value) == 8:
        dt = datetime.strptime(value[:8], "%Y%m%d").replace(tzinfo=KST)
        return dt, True
    if value.endswith("Z"):
        dt = datetime.strptime(value, "%Y%m%dT%H%M%SZ").replace(tzinfo=UTC)
        return dt.astimezone(KST), False
    if len(value) == 15:
        dt = datetime.strptime(value, "%Y%m%dT%H%M%S").replace(tzinfo=zone)
        return dt.astimezone(KST), False
    if len(value) == 13:
        dt = datetime.strptime(value, "%Y%m%dT%H%M").replace(tzinfo=zone)
        return dt.astimezone(KST), False
    raise ValueError(f"Unsupported ICS datetime value: {value}")


def parse_exdates(values: list[tuple[dict[str, str], str]]) -> set[datetime]:
    dates: set[datetime] = set()
    for params, raw_value in values:
        for part in raw_value.split(","):
            try:
                dt, _ = parse_ics_datetime(part, params)
            except ValueError:
                continue
            dates.add(dt)
    return dates


def parse_ics_events(text: str, now: datetime, window_days: int, max_events: int) -> list[ScheduleEvent]:
    lines = unfold_ics_lines(text)
    blocks: list[list[str]] = []
    current: list[str] | None = None
    for line in lines:
        if line == "BEGIN:VEVENT":
            current = []
            continue
        if line == "END:VEVENT":
            if current is not None:
                blocks.append(current)
            current = None
            continue
        if current is not None:
            current.append(line)

    window_start = now - timedelta(hours=4)
    window_end = now + timedelta(days=max(1, window_days))
    expanded: list[ScheduleEvent] = []

    for block in blocks:
        props: dict[str, list[tuple[dict[str, str], str]]] = {}
        for line in block:
            if ":" not in line:
                continue
            name, params, value = split_content_line(line)
            props.setdefault(name, []).append((params, value))

        if "DTSTART" not in props or "SUMMARY" not in props:
            continue

        start_params, start_value = props["DTSTART"][0]
        start, all_day = parse_ics_datetime(start_value, start_params)

        if "DTEND" in props:
            end_params, end_value = props["DTEND"][0]
            end, _ = parse_ics_datetime(end_value, end_params)
        else:
            end = start + (timedelta(days=1) if all_day else timedelta(hours=1))

        duration = end - start
        summary = clean_text(props["SUMMARY"][0][1], max_len=40)
        location = clean_text(props.get("LOCATION", [({}, "")])[0][1], max_len=32)
        description = clean_text(props.get("DESCRIPTION", [({}, "")])[0][1], max_len=80)
        exdates = parse_exdates(props.get("EXDATE", []))
        rrule_values = props.get("RRULE", [])

        starts: list[datetime]
        if rrule_values:
            rule = rrulestr(rrule_values[0][1], dtstart=start)
            starts = [
                occurrence.astimezone(KST) if occurrence.tzinfo else occurrence.replace(tzinfo=KST)
                for occurrence in rule.between(window_start - duration, window_end, inc=True)
            ]
        else:
            starts = [start]

        for occurrence_start in starts:
            if occurrence_start in exdates:
                continue
            occurrence_end = occurrence_start + duration
            if occurrence_end < window_start or occurrence_start > window_end:
                continue
            expanded.append(
                ScheduleEvent(
                    start=occurrence_start,
                    end=occurrence_end,
                    summary=summary,
                    location=location,
                    description=description,
                    all_day=all_day,
                )
            )

    expanded.sort(key=lambda event: event.start)
    return expanded[: max_events]


def event_value(event: ScheduleEvent) -> str:
    if event.all_day:
        return clean_text(f"{event.summary} · 종일", max_len=40)
    return clean_text(f"{format_korean_datetime(event.start)} {event.summary}", max_len=40)


def event_detail(event: ScheduleEvent) -> str:
    parts = [format_korean_date(event.start)]
    if event.all_day:
        parts.append("종일")
    else:
        parts.append(f"{format_korean_datetime(event.start)}-{format_korean_datetime(event.end)}")
    if event.location:
        parts.append(event.location)
    elif event.description:
        parts.append(event.description)
    return clean_text(" · ".join(parts), max_len=72)


def normalize_ics(text: str, now: datetime, window_days: int, max_events: int) -> dict[str, object]:
    events = parse_ics_events(text, now, window_days, max_events)
    if not events:
        raise RuntimeError("No schedule events found in the selected window.")

    current_events = [event for event in events if event.start <= now < event.end]
    upcoming_events = [event for event in events if event.end > now]
    if not upcoming_events:
        raise RuntimeError("No schedule events found in the selected window.")
    today_end = now.replace(hour=23, minute=59, second=59, microsecond=0)
    today_events = [event for event in upcoming_events if event.start <= today_end]
    later_events = [event for event in upcoming_events if event.start > today_end]

    next_event = upcoming_events[0]
    primary_metrics = [
        {"label": "다음 일정", "value": event_value(next_event), "detail": event_detail(next_event)},
        {"label": "오늘 남은 일정", "value": f"{len(today_events)}건", "detail": format_korean_date(now)},
        {
            "label": "현재 진행",
            "value": current_events[0].summary if current_events else "없음",
            "detail": event_detail(current_events[0]) if current_events else "현재 진행 중인 일정 없음",
        },
    ]

    sections = []
    if current_events or upcoming_events:
        sections.append(
            {
                "title": "지금과 다음",
                "items": [
                    {
                        "icon": "schedule",
                        "label": "현재",
                        "value": current_events[0].summary if current_events else "진행 중 일정 없음",
                        "detail": event_detail(current_events[0]) if current_events else "다음 일정만 준비합니다.",
                    },
                    {
                        "icon": "event",
                        "label": "다음",
                        "value": next_event.summary,
                        "detail": event_detail(next_event),
                    },
                ],
            }
        )
    if today_events:
        sections.append(
            {
                "title": "오늘 일정",
                "items": [
                    {
                        "icon": "eventAvailable" if not event.all_day else "today",
                        "label": event.summary,
                        "value": "종일" if event.all_day else format_korean_datetime(event.start),
                        "detail": event_detail(event),
                    }
                    for event in today_events[:4]
                ],
            }
        )
    if later_events:
        sections.append(
            {
                "title": "이후 일정",
                "items": [
                    {
                        "icon": "eventNote",
                        "label": event.summary,
                        "value": format_korean_date(event.start),
                        "detail": event_detail(event),
                    }
                    for event in later_events[:3]
                ],
            }
        )

    payload = {
        "title": "오늘 일정 브리핑",
        "headline": clean_text("ICS 일정 피드에서 현재와 다음 일정을 TV 거리에서 읽히는 형태로 정리했습니다.", max_len=88),
        "primaryMetrics": primary_metrics,
        "sections": sections,
        "alert": {
            "icon": "visibilityOff",
            "title": "공용 화면 주의",
            "summary": "참석자 이름, 상세 메모, 정확한 위치는 기본적으로 축약하거나 감춘 형태로 표시하는 편이 안전합니다.",
            "meta": "ICS 일정 요약",
        },
        "actions": [
            {"label": "새로고침", "event": "refreshSchedule"},
            {"label": "내일 보기", "event": "showTomorrowSchedule"},
        ],
        "footer": "현재 live adapter는 ICS URL 또는 파일을 지원합니다. 반복 일정은 RRULE 기준으로 현재 창 안에서 확장합니다.",
    }
    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "ics-url":
            payload = normalize_ics(
                fetch_text(args.ics_url),
                parse_now(args.now),
                args.days,
                args.max_events,
            )
        elif args.source == "ics-file":
            if not args.ics_file:
                raise RuntimeError("--ics-file is required when --source ics-file is selected.")
            payload = normalize_ics(
                args.ics_file.read_text(encoding="utf-8"),
                parse_now(args.now),
                args.days,
                args.max_events,
            )
        else:
            payload = load_payload(args.input)
    except Exception as exc:
        message = clean_text(str(exc), max_len=88)
        title = SPEC.error_title
        hint = SPEC.error_hint
        if "No schedule events" in str(exc):
            title = SPEC.empty_title
            hint = SPEC.empty_hint
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=title,
                detail=message or SPEC.error_detail,
                hint=hint,
                button_label="다시 시도" if title == SPEC.error_title else None,
                button_event=SPEC.retry_event if title == SPEC.error_title else None,
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
