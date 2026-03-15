#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import KST, fetch_json, format_korean_date, format_korean_datetime
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


LEAGUE_PRESETS = {
    "kleague1": {"id": "4689", "name": "K League 1"},
    "kleague2": {"id": "4822", "name": "K League 2"},
    "kbo": {"id": "4830", "name": "KBO League"},
}

SPEC = ScenarioSpec(
    skill_name="tv-sports-briefing",
    title="TV Sports Briefing",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="sports_main",
    loading_title="경기 정보 준비 중",
    loading_detail="점수판과 다음 경기 정보를 정리하고 있습니다.",
    loading_hint="메인 경기와 순위 변동을 먼저 채웁니다.",
    empty_title="표시할 경기 없음",
    empty_detail="현재 표시할 스포츠 카드가 아직 없습니다.",
    empty_hint="리그 선택 또는 mock 경기 데이터를 먼저 확인해 주세요.",
    error_title="경기 정보를 불러오지 못했습니다",
    error_detail="스코어 피드 연결 또는 리그 데이터 정규화를 확인해 주세요.",
    error_hint="라이브 여부와 마지막 갱신 시각을 명확히 보여주는 편이 안전합니다.",
    retry_event="refreshSports",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV sports briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "thesportsdb"],
        default="mock",
        help="Choose whether to use the bundled mock file or the live TheSportsDB feed.",
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
        "--league",
        choices=sorted(LEAGUE_PRESETS),
        default="kleague1",
        help="Preset league used when --source thesportsdb is selected.",
    )
    parser.add_argument(
        "--league-id",
        help="Optional explicit TheSportsDB league ID override.",
    )
    parser.add_argument(
        "--league-name",
        help="Optional display name override for the chosen league.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized sports payload before A2UI generation.",
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


def parse_event_datetime(event: dict[str, object]) -> datetime | None:
    date_value = event.get("dateEventLocal") or event.get("dateEvent")
    time_value = event.get("strTimeLocal") or event.get("strTime") or "00:00:00"
    if not isinstance(date_value, str):
        return None
    if not isinstance(time_value, str):
        time_value = "00:00:00"
    iso_value = f"{date_value}T{time_value}"
    try:
        return datetime.fromisoformat(iso_value).replace(tzinfo=KST)
    except ValueError:
        return None


def is_finished(event: dict[str, object]) -> bool:
    status = clean_text(event.get("strStatus"), max_len=32).lower()
    return status in {"ft", "match finished", "aet", "pen"} or (
        event.get("intHomeScore") is not None and event.get("intAwayScore") is not None
    )


def event_label(event: dict[str, object]) -> str:
    return clean_text(
        f"{event.get('strHomeTeam', '홈')} vs {event.get('strAwayTeam', '원정')}",
        max_len=40,
    )


def event_value(event: dict[str, object]) -> str:
    home = clean_text(event.get("strHomeTeam"), max_len=18)
    away = clean_text(event.get("strAwayTeam"), max_len=18)
    home_score = event.get("intHomeScore")
    away_score = event.get("intAwayScore")
    if home_score is not None and away_score is not None:
        return clean_text(f"{home} {home_score} : {away_score} {away}", max_len=44)
    dt = parse_event_datetime(event)
    if dt is None:
        return clean_text(f"{home} vs {away}", max_len=44)
    return clean_text(f"{home} vs {away} {format_korean_datetime(dt)}", max_len=44)


def event_detail(event: dict[str, object]) -> str:
    dt = parse_event_datetime(event)
    status = clean_text(event.get("strStatus"), max_len=20) or "상태 확인 필요"
    venue = clean_text(event.get("strVenue"), max_len=28)
    if dt is None:
        return clean_text(status if not venue else f"{status} · {venue}", max_len=60)
    base = f"{format_korean_date(dt)} {format_korean_datetime(dt)} · {status}"
    if venue:
        base = f"{base} · {venue}"
    return clean_text(base, max_len=60)


def dedupe_events(events: list[dict[str, object]]) -> list[dict[str, object]]:
    seen: set[str] = set()
    unique: list[dict[str, object]] = []
    for event in events:
        event_id = clean_text(event.get("idEvent"), max_len=24)
        if event_id and event_id in seen:
            continue
        if event_id:
            seen.add(event_id)
        unique.append(event)
    return unique


def normalize_thesportsdb(league_id: str, league_name: str) -> dict[str, object]:
    next_url = f"https://www.thesportsdb.com/api/v1/json/123/eventsnextleague.php?id={league_id}"
    past_url = f"https://www.thesportsdb.com/api/v1/json/123/eventspastleague.php?id={league_id}"
    next_events = fetch_json(next_url).get("events") or []
    past_events = fetch_json(past_url).get("events") or []
    all_events = dedupe_events(
        [event for event in [*past_events, *next_events] if isinstance(event, dict)]
    )
    now = datetime.now(KST)
    recent = sorted(
        [event for event in all_events if is_finished(event)],
        key=lambda event: parse_event_datetime(event) or now,
        reverse=True,
    )
    upcoming = sorted(
        [event for event in all_events if not is_finished(event)],
        key=lambda event: parse_event_datetime(event) or now,
    )

    hero = recent[0] if recent else (upcoming[0] if upcoming else None)
    if hero is None:
        raise RuntimeError("No sports events found.")

    primary_metrics = [
        {"label": "리그", "value": league_name, "detail": "실시간 피드"},
    ]
    if recent:
        primary_metrics.append(
            {
                "label": "최근 결과",
                "value": event_value(recent[0]),
                "detail": event_detail(recent[0]),
            }
        )
    if upcoming:
        primary_metrics.append(
            {
                "label": "다음 경기",
                "value": event_label(upcoming[0]),
                "detail": event_detail(upcoming[0]),
            }
        )
    else:
        primary_metrics.append(
            {
                "label": "다음 경기",
                "value": "일정 확인 필요",
                "detail": "현재 피드 기준 예정 경기 없음",
            }
        )

    sections = []
    if recent:
        sections.append(
            {
                "title": "최근 경기",
                "items": [
                    {
                        "icon": "sportsSoccer" if "K League" in league_name else "sportsBaseball",
                        "label": event_label(event),
                        "value": event_value(event),
                        "detail": event_detail(event),
                    }
                    for event in recent[:3]
                ],
            }
        )
    if upcoming:
        sections.append(
            {
                "title": "예정 경기",
                "items": [
                    {
                        "icon": "schedule",
                        "label": event_label(event),
                        "value": event_value(event),
                        "detail": event_detail(event),
                    }
                    for event in upcoming[:3]
                ],
            }
        )

    payload = {
        "title": "오늘의 스포츠 브리핑",
        "headline": clean_text(f"{league_name} 기준 최신 경기와 다음 일정을 정리했습니다.", max_len=88),
        "primaryMetrics": primary_metrics[:3],
        "sections": sections,
        "alert": {
            "icon": "update",
            "title": "갱신 시각 표시 필요",
            "summary": "실시간 경기로 보이는 순간 stale data에 민감해지므로 마지막 갱신 시각과 리그 출처를 함께 보여주는 편이 안전합니다.",
            "meta": "TheSportsDB",
        },
        "actions": [
            {"label": "새로고침", "event": "refreshSports"},
            {"label": "리그 변경", "event": "changeLeague"},
        ],
        "footer": "기본 live preset은 한국 리그 중심이며, 더 정확한 라이브 스코어가 필요하면 전용 유료 피드 또는 공식 제휴 소스를 검토하는 편이 좋습니다.",
    }
    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "thesportsdb":
            preset = LEAGUE_PRESETS[args.league]
            league_id = args.league_id or preset["id"]
            league_name = args.league_name or preset["name"]
            payload = normalize_thesportsdb(league_id, league_name)
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
