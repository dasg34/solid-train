#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from datetime import datetime, timedelta
from pathlib import Path
from types import ModuleType
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


DEFAULT_ICS_URL = "https://holidays.hyunbin.page/basic.ics"
ROOT_DIR = Path(__file__).resolve().parents[2]
DEFAULT_INPUT_PATH = Path(__file__).resolve().parents[1] / "references" / "mock_surface.json"
WEATHER_MOCK_PATH = ROOT_DIR / "tv-weather-briefing" / "references" / "mock_weather_seoul.json"
NEWS_MOCK_PATH = ROOT_DIR / "tv-news-briefing" / "references" / "mock_surface.json"
SCHEDULE_MOCK_PATH = ROOT_DIR / "tv-schedule-briefing" / "references" / "mock_surface.json"
COMMUTE_MOCK_PATH = ROOT_DIR / "tv-commute-briefing" / "references" / "mock_surface.json"

WEATHER_SCRIPT_PATH = ROOT_DIR / "tv-weather-briefing" / "scripts" / "generate_weather_a2ui.py"
NEWS_SCRIPT_PATH = ROOT_DIR / "tv-news-briefing" / "scripts" / "generate_news_a2ui.py"
SCHEDULE_SCRIPT_PATH = ROOT_DIR / "tv-schedule-briefing" / "scripts" / "generate_schedule_a2ui.py"
COMMUTE_SCRIPT_PATH = ROOT_DIR / "tv-commute-briefing" / "scripts" / "generate_commute_a2ui.py"

MODULE_CACHE: dict[str, ModuleType] = {}

SPEC = ScenarioSpec(
    skill_name="tv-daily-briefing",
    title="TV Daily Briefing",
    default_input=DEFAULT_INPUT_PATH,
    default_surface_id="daily_briefing_main",
    loading_title="오늘 브리핑 준비 중",
    loading_detail="날씨, 일정, 출근, 뉴스 카드를 순서대로 모으고 있습니다.",
    loading_hint="기본 카드부터 표시하고 나머지는 순차적으로 채웁니다.",
    empty_title="표시할 브리핑 없음",
    empty_detail="아직 조합할 요약 카드가 없습니다.",
    empty_hint="weather, news, schedule, commute source 설정을 다시 확인해 주세요.",
    error_title="오늘 브리핑을 불러오지 못했습니다",
    error_detail="조합형 카드 생성 또는 부분 실패 처리 상태를 확인해 주세요.",
    error_hint="일부 소스가 실패해도 남은 카드만으로 일관된 화면을 유지하는 편이 좋습니다.",
    retry_event="refreshDailyBriefing",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV daily briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "compose-live"],
        default="mock",
        help="Use the bundled daily mock payload or compose a dashboard from live adapters.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SPEC.default_input,
        help="Path to a normalized daily briefing JSON file used for --source mock.",
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
        "--weather-source",
        choices=["open-meteo", "mock", "skip"],
        default="open-meteo",
        help="Source to use for the weather card in compose-live mode.",
    )
    parser.add_argument(
        "--news-source",
        choices=["yonhap-rss", "mock", "skip"],
        default="yonhap-rss",
        help="Source to use for the news card in compose-live mode.",
    )
    parser.add_argument(
        "--schedule-source",
        choices=["ics-url", "ics-file", "mock", "skip"],
        default="ics-url",
        help="Source to use for the schedule card in compose-live mode.",
    )
    parser.add_argument(
        "--commute-source",
        choices=["osrm", "mock", "skip"],
        default="osrm",
        help="Source to use for the commute card in compose-live mode.",
    )
    parser.add_argument(
        "--latitude",
        type=float,
        default=37.5665,
        help="Latitude used when weather-source is open-meteo.",
    )
    parser.add_argument(
        "--longitude",
        type=float,
        default=126.9780,
        help="Longitude used when weather-source is open-meteo.",
    )
    parser.add_argument(
        "--city",
        default="서울",
        help="City label used for the weather card.",
    )
    parser.add_argument(
        "--district",
        default="중구",
        help="District label used for the weather card.",
    )
    parser.add_argument(
        "--weather-hours",
        type=int,
        default=4,
        help="Number of upcoming hourly rows to request for the weather card.",
    )
    parser.add_argument(
        "--rss-url",
        default="https://www.yonhapnewstv.co.kr/browse/feed/",
        help="RSS URL used when news-source is yonhap-rss.",
    )
    parser.add_argument(
        "--news-count",
        type=int,
        default=5,
        help="Number of news items to include from the RSS feed.",
    )
    parser.add_argument(
        "--ics-url",
        default=DEFAULT_ICS_URL,
        help="ICS URL used when schedule-source is ics-url.",
    )
    parser.add_argument(
        "--ics-file",
        type=Path,
        help="ICS file used when schedule-source is ics-file.",
    )
    parser.add_argument(
        "--schedule-days",
        type=int,
        default=2,
        help="Number of days ahead to include from the live schedule feed.",
    )
    parser.add_argument(
        "--schedule-max-events",
        type=int,
        default=6,
        help="Maximum number of expanded schedule events to include.",
    )
    parser.add_argument(
        "--schedule-now",
        help="Optional ISO-8601 override passed to the schedule adapter.",
    )
    parser.add_argument(
        "--commute-origin",
        default="서울시청",
        help="Origin query used for the commute card when commute-source is osrm.",
    )
    parser.add_argument(
        "--commute-destination",
        default="강남역",
        help="Destination query used for the commute card when commute-source is osrm.",
    )
    parser.add_argument(
        "--commute-origin-label",
        help="Optional masked origin label for the commute card.",
    )
    parser.add_argument(
        "--commute-destination-label",
        help="Optional masked destination label for the commute card.",
    )
    parser.add_argument(
        "--commute-profile",
        choices=["driving", "walking"],
        default="driving",
        help="OSRM profile used for the commute card.",
    )
    parser.add_argument(
        "--commute-arrive-by",
        help="Optional ISO-8601 arrival target used for the commute card.",
    )
    parser.add_argument(
        "--commute-now",
        help="Optional ISO-8601 current-time override used for the commute card.",
    )
    parser.add_argument(
        "--commute-buffer-minutes",
        type=int,
        default=8,
        help="Extra arrival buffer used for the commute card.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized daily payload before A2UI generation.",
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


def load_script_module(name: str, path: Path) -> ModuleType:
    cached = MODULE_CACHE.get(name)
    if cached is not None:
        return cached
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load helper module from {path}.")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    MODULE_CACHE[name] = module
    return module


def weather_module() -> ModuleType:
    return load_script_module("tv_daily_weather_skill", WEATHER_SCRIPT_PATH)


def news_module() -> ModuleType:
    return load_script_module("tv_daily_news_skill", NEWS_SCRIPT_PATH)


def schedule_module() -> ModuleType:
    return load_script_module("tv_daily_schedule_skill", SCHEDULE_SCRIPT_PATH)


def commute_module() -> ModuleType:
    return load_script_module("tv_daily_commute_skill", COMMUTE_SCRIPT_PATH)


def safe_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def safe_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def short_value(value: Any, *, max_len: int = 32) -> str:
    return clean_text(value, max_len=max_len)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise RuntimeError(f"{path.name} must contain a JSON object.")
    return data


def load_weather_payload(args: argparse.Namespace) -> tuple[dict[str, Any] | None, str | None]:
    if args.weather_source == "skip":
        return None, None
    if args.weather_source == "mock":
        return read_json(WEATHER_MOCK_PATH), "mock"
    module = weather_module()
    payload = module.fetch_open_meteo_payload(
        latitude=args.latitude,
        longitude=args.longitude,
        city=args.city,
        district=args.district,
        hours=max(1, min(args.weather_hours, 6)),
    )
    return payload, "Open-Meteo"


def load_news_payload(args: argparse.Namespace) -> tuple[dict[str, Any] | None, str | None]:
    if args.news_source == "skip":
        return None, None
    if args.news_source == "mock":
        return read_json(NEWS_MOCK_PATH), "mock"
    module = news_module()
    payload = module.normalize_yonhap_rss(module.fetch_text(args.rss_url), args.news_count)
    return payload, "연합뉴스TV RSS"


def load_schedule_payload(args: argparse.Namespace) -> tuple[dict[str, Any] | None, str | None, str | None]:
    if args.schedule_source == "skip":
        return None, None, None
    if args.schedule_source == "mock":
        return read_json(SCHEDULE_MOCK_PATH), "mock", None

    module = schedule_module()
    now = module.parse_now(args.schedule_now)
    if args.schedule_source == "ics-file":
        if args.ics_file is None:
            raise RuntimeError("--ics-file is required when --schedule-source ics-file is used.")
        text = args.ics_file.read_text(encoding="utf-8")
        source_label = "ICS file"
    else:
        text = module.fetch_text(args.ics_url)
        source_label = "ICS URL"

    try:
        payload = module.normalize_ics(
            text,
            now,
            max(1, args.schedule_days),
            max(1, args.schedule_max_events),
        )
    except RuntimeError as exc:
        message = clean_text(str(exc), max_len=80)
        if "No schedule events found" in message:
            return None, source_label, "empty"
        raise
    return payload, source_label, None


def parse_commute_arrive_by(value: str | None) -> datetime | None:
    if not value:
        return None
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        return dt
    return dt


def extract_schedule_arrive_by(payload: dict[str, Any] | None) -> str | None:
    metric = metric_matching(payload or {}, "다음") or first_metric(payload or {}, 0)
    text = short_value(metric.get("value"), max_len=40)
    now = datetime.now().astimezone()

    def build_target(hour_value: int, minute_value: int) -> str:
        target = now.replace(
            hour=hour_value,
            minute=minute_value,
            second=0,
            microsecond=0,
        )
        if target <= now:
            target += timedelta(days=1)
        return target.isoformat()

    for prefix in ("오전 ", "오후 "):
        if text.startswith(prefix):
            token = text.split(" ", 2)[:2]
            if len(token) == 2 and ":" in token[1]:
                hour, minute = token[1].split(":", 1)
                try:
                    parsed_hour = int(hour) % 12
                    if prefix == "오후 ":
                        parsed_hour += 12
                    return build_target(parsed_hour, int(minute))
                except ValueError:
                    return None
    leading = text.split(" ", 1)[0]
    if ":" in leading:
        hour, minute = leading.split(":", 1)
        try:
            return build_target(int(hour), int(minute))
        except ValueError:
            return None
    return None


def load_commute_payload(
    args: argparse.Namespace,
    schedule_payload: dict[str, Any] | None,
) -> tuple[dict[str, Any] | None, str | None]:
    if args.commute_source == "skip":
        return None, None
    if args.commute_source == "mock":
        return read_json(COMMUTE_MOCK_PATH), "mock"
    module = commute_module()
    arrive_by = args.commute_arrive_by or extract_schedule_arrive_by(schedule_payload)
    payload = module.normalize_osrm_commute(
        origin_query=args.commute_origin,
        destination_query=args.commute_destination,
        origin_label=args.commute_origin_label,
        destination_label=args.commute_destination_label,
        profile=args.commute_profile,
        arrive_by=arrive_by,
        now_value=args.commute_now,
        buffer_minutes=args.commute_buffer_minutes,
    )
    return payload, "Nominatim + OSRM"


def first_metric(payload: dict[str, Any], index: int) -> dict[str, Any]:
    metrics = [metric for metric in safe_list(payload.get("primaryMetrics")) if isinstance(metric, dict)]
    if 0 <= index < len(metrics):
        return metrics[index]
    return {}


def metric_matching(payload: dict[str, Any], keyword: str) -> dict[str, Any]:
    for metric in safe_list(payload.get("primaryMetrics")):
        if not isinstance(metric, dict):
            continue
        label = short_value(metric.get("label"), max_len=24)
        if keyword in label:
            return metric
    return {}


def first_items(payload: dict[str, Any], *, max_items: int) -> list[dict[str, Any]]:
    collected: list[dict[str, Any]] = []
    for section in safe_list(payload.get("sections")):
        if not isinstance(section, dict):
            continue
        for item in safe_list(section.get("items")):
            if isinstance(item, dict):
                collected.append(item)
            if len(collected) >= max_items:
                return collected
    return collected


def compose_weather_section(payload: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], str]:
    module = weather_module()
    location = safe_dict(payload.get("location"))
    city = short_value(location.get("city"), max_len=12) or "서울"
    district = short_value(location.get("district"), max_len=12)
    current = safe_dict(payload.get("current"))
    current_condition = short_value(current.get("condition"), max_len=18) or "날씨 정보"
    current_temp = module.format_temperature(current.get("temperature_c"))
    feels_like = module.format_temperature(current.get("feels_like_c"))
    precip = module.format_percent(current.get("precip_probability_pct"))
    hourly = [hour for hour in safe_list(payload.get("hourly")) if isinstance(hour, dict)]

    items = [
        {
            "icon": module.icon_name_for_condition(current_condition),
            "label": "현재",
            "value": short_value(f"{current_condition} {current_temp}", max_len=28),
            "detail": short_value(
                f"{district or city} · 체감 {feels_like} · 강수확률 {precip}",
                max_len=72,
            ),
        }
    ]
    for hour in hourly[:2]:
        hour_condition = short_value(hour.get("condition"), max_len=18) or "날씨"
        items.append(
            {
                "icon": module.icon_name_for_condition(hour_condition),
                "label": module.format_hour_label(hour),
                "value": short_value(
                    f"{hour_condition} {module.format_temperature(hour.get('temperature_c'))}",
                    max_len=28,
                ),
                "detail": short_value(
                    f"강수확률 {module.format_percent(hour.get('precip_probability_pct'))}",
                    max_len=64,
                ),
            }
        )

    metric = {
        "label": "현재 날씨",
        "value": short_value(f"{current_condition} {current_temp}", max_len=28),
        "detail": short_value(f"체감 {feels_like}", max_len=32),
    }
    section = {"title": "오늘 날씨", "items": items}
    snippet = short_value(
        payload.get("headline") or f"{city} 현재 {current_condition} {current_temp}",
        max_len=36,
    )
    return section, metric, snippet


def compose_news_section(payload: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], str]:
    metric_source = first_metric(payload, 1) or first_metric(payload, 0)
    items = []
    for item in first_items(payload, max_items=3):
        items.append(
            {
                "icon": short_value(item.get("icon"), max_len=20) or "article",
                "label": short_value(item.get("label"), max_len=20) or "최신",
                "value": short_value(item.get("value"), max_len=30),
                "detail": short_value(item.get("detail"), max_len=72),
            }
        )
    lead = items[0] if items else {"value": "주요 뉴스를 준비 중입니다."}
    metric = {
        "label": short_value(metric_source.get("label"), max_len=20) or "헤드라인",
        "value": short_value(metric_source.get("value"), max_len=20) or f"{len(items)}건",
        "detail": short_value(metric_source.get("detail"), max_len=32) or "뉴스 카드",
    }
    section = {"title": "주요 뉴스", "items": items[:3]}
    snippet = short_value(lead.get("value"), max_len=36)
    return section, metric, snippet


def compose_schedule_section(payload: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], str]:
    section_items = []
    for item in first_items(payload, max_items=3):
        section_items.append(
            {
                "icon": short_value(item.get("icon"), max_len=20) or "schedule",
                "label": short_value(item.get("label"), max_len=18),
                "value": short_value(item.get("value"), max_len=30),
                "detail": short_value(item.get("detail"), max_len=72),
            }
        )
    metric_source = metric_matching(payload, "다음") or first_metric(payload, 0)
    section = {"title": "다음 일정", "items": section_items[:3]}
    metric = {
        "label": short_value(metric_source.get("label"), max_len=20) or "다음 일정",
        "value": short_value(metric_source.get("value"), max_len=28) or "일정 확인 필요",
        "detail": short_value(metric_source.get("detail"), max_len=32) or "캘린더 연결 상태 확인",
    }
    snippet = short_value(f"{metric['label']} {metric['value']}", max_len=36)
    return section, metric, snippet


def compose_commute_section(payload: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any], str]:
    metric_source = metric_matching(payload, "추천") or first_metric(payload, 0)
    section_items = []
    for item in first_items(payload, max_items=3):
        section_items.append(
            {
                "icon": short_value(item.get("icon"), max_len=20) or "traffic",
                "label": short_value(item.get("label"), max_len=18),
                "value": short_value(item.get("value"), max_len=30),
                "detail": short_value(item.get("detail"), max_len=72),
            }
        )
    section = {"title": "출근", "items": section_items[:3]}
    metric = {
        "label": short_value(metric_source.get("label"), max_len=20) or "추천 출발",
        "value": short_value(metric_source.get("value"), max_len=28) or "경로 확인 필요",
        "detail": short_value(metric_source.get("detail"), max_len=32) or "이동 카드",
    }
    snippet = short_value(payload.get("headline") or f"{metric['label']} {metric['value']}", max_len=36)
    return section, metric, snippet


def compose_alert(
    *,
    weather_payload: dict[str, Any] | None,
    missing_cards: list[str],
    partial_reasons: list[str],
    sources_used: list[str],
) -> dict[str, str]:
    if missing_cards:
        summary = " / ".join(reason for reason in partial_reasons if reason)
        if not summary:
            summary = "일부 카드를 준비하지 못해 표시 가능한 카드만 먼저 보여줍니다."
        return {
            "icon": "dashboard",
            "title": "일부 카드만 표시 중",
            "summary": short_value(summary, max_len=100),
            "meta": short_value(", ".join(missing_cards), max_len=40),
        }

    weather_alert = safe_dict(safe_dict(weather_payload).get("alert"))
    if weather_alert.get("title") or weather_alert.get("summary"):
        return {
            "icon": "warning",
            "title": short_value(weather_alert.get("title"), max_len=28) or "날씨 알림",
            "summary": short_value(weather_alert.get("summary"), max_len=100),
            "meta": short_value(weather_alert.get("source"), max_len=40),
        }

    return {
        "icon": "info",
        "title": "출처와 시각 유지",
        "summary": "조합형 브리핑은 각 카드의 출처와 갱신 시각을 따로 유지하는 편이 신뢰 판단에 유리합니다.",
        "meta": short_value(" · ".join(sources_used), max_len=48),
    }


def compose_footer(sources_used: list[str], partial_reasons: list[str]) -> str:
    base = "기본 순서는 날씨, 일정, 출근, 뉴스이며 각 카드는 TV 거리에서 빠르게 읽히는 길이로 축약합니다."
    if sources_used:
        base = f"{base} 사용 소스: {', '.join(sources_used)}."
    if partial_reasons:
        base = f"{base} 일부 카드 상태: {' / '.join(partial_reasons)}."
    return short_value(base, max_len=100)


def compose_live_payload(args: argparse.Namespace) -> dict[str, Any]:
    partial_reasons: list[str] = []
    missing_cards: list[str] = []
    sources_used: list[str] = []

    weather_payload: dict[str, Any] | None = None
    news_payload: dict[str, Any] | None = None
    schedule_payload: dict[str, Any] | None = None
    commute_payload: dict[str, Any] | None = None
    weather_card: tuple[dict[str, Any], dict[str, Any], str] | None = None
    news_card: tuple[dict[str, Any], dict[str, Any], str] | None = None
    schedule_card: tuple[dict[str, Any], dict[str, Any], str] | None = None
    commute_card: tuple[dict[str, Any], dict[str, Any], str] | None = None

    try:
        weather_payload, weather_source = load_weather_payload(args)
    except Exception as exc:
        weather_source = None
        missing_cards.append("날씨")
        partial_reasons.append(f"날씨 연결 실패: {short_value(exc, max_len=44)}")
    else:
        if weather_payload:
            weather_card = compose_weather_section(weather_payload)
            if weather_source:
                sources_used.append(f"날씨 {weather_source}")
        elif args.weather_source != "skip":
            missing_cards.append("날씨")
            partial_reasons.append("날씨 카드가 비어 있습니다.")

    try:
        news_payload, news_source = load_news_payload(args)
    except Exception as exc:
        news_source = None
        missing_cards.append("뉴스")
        partial_reasons.append(f"뉴스 연결 실패: {short_value(exc, max_len=44)}")
    else:
        if news_payload:
            news_card = compose_news_section(news_payload)
            if news_source:
                sources_used.append(f"뉴스 {news_source}")
        elif args.news_source != "skip":
            missing_cards.append("뉴스")
            partial_reasons.append("뉴스 카드가 비어 있습니다.")

    try:
        schedule_payload, schedule_source, schedule_status = load_schedule_payload(args)
    except Exception as exc:
        schedule_source = None
        schedule_status = "error"
        missing_cards.append("일정")
        partial_reasons.append(f"일정 연결 실패: {short_value(exc, max_len=44)}")
    else:
        if schedule_payload:
            schedule_card = compose_schedule_section(schedule_payload)
            if schedule_source:
                sources_used.append(f"일정 {schedule_source}")
        elif args.schedule_source != "skip":
            missing_cards.append("일정")
            if schedule_status == "empty":
                partial_reasons.append("현재 창에서는 일정 카드가 비어 있습니다.")
            else:
                partial_reasons.append("일정 카드가 비어 있습니다.")
            if schedule_source:
                sources_used.append(f"일정 {schedule_source}")

    try:
        commute_payload, commute_source = load_commute_payload(args, schedule_payload)
    except Exception as exc:
        commute_source = None
        missing_cards.append("출근")
        partial_reasons.append(f"출근 연결 실패: {short_value(exc, max_len=44)}")
    else:
        if commute_payload:
            commute_card = compose_commute_section(commute_payload)
            if commute_source:
                sources_used.append(f"출근 {commute_source}")
        elif args.commute_source != "skip":
            missing_cards.append("출근")
            partial_reasons.append("출근 카드가 비어 있습니다.")

    sections: list[dict[str, Any]] = []
    metrics: list[dict[str, Any]] = []
    headline_parts: list[str] = []
    available_actions = [
        ("날씨", "openWeatherCard", weather_card),
        ("일정", "openScheduleCard", schedule_card),
        ("출근", "openCommuteCard", commute_card),
        ("뉴스", "openNewsCard", news_card),
    ]
    for card in (weather_card, schedule_card, commute_card, news_card):
        if card is None:
            continue
        section, metric, snippet = card
        sections.append(section)
        metrics.append(metric)
        headline_parts.append(snippet)

    if not sections:
        raise RuntimeError("표시 가능한 live 카드가 없습니다.")

    actions = [{"label": "새로고침", "event": "refreshDailyBriefing"}]
    for label, event, card in available_actions:
        if card is None or len(actions) >= 3:
            continue
        actions.append({"label": label, "event": event})

    title = "오늘의 브리핑"
    headline = short_value(
        " · ".join(part for part in headline_parts if part) or "오늘 아침에 필요한 핵심 카드만 추렸습니다.",
        max_len=88,
    )
    payload = {
        "title": title,
        "headline": headline,
        "primaryMetrics": metrics[:3],
        "sections": sections[:4],
        "alert": compose_alert(
            weather_payload=weather_payload,
            missing_cards=missing_cards,
            partial_reasons=partial_reasons,
            sources_used=sources_used,
        ),
        "actions": actions,
        "footer": compose_footer(sources_used, partial_reasons),
    }

    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "compose-live":
            payload = compose_live_payload(args)
        else:
            payload = load_payload(args.input)
    except Exception as exc:
        message = short_value(str(exc), max_len=88)
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
