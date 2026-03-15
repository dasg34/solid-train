#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from urllib.parse import urlencode
from datetime import datetime
from pathlib import Path
from typing import Any


DEFAULT_CATALOG_ID = "https://a2ui.org/specification/v0_9/standard_catalog.json"
DEFAULT_INPUT_PATH = (
    Path(__file__).resolve().parents[1] / "references" / "mock_weather_seoul.json"
)
DEFAULT_SURFACE_ID = "weather_main"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI weather payloads for the TV weather briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "weather", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "open-meteo"],
        default="mock",
        help="Choose whether to use the bundled mock file or fetch live data from Open-Meteo.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=DEFAULT_INPUT_PATH,
        help="Path to a normalized weather JSON file. Used for --state weather with --source mock.",
    )
    parser.add_argument(
        "--surface-id",
        default=DEFAULT_SURFACE_ID,
        help="Surface ID to use in the emitted A2UI messages.",
    )
    parser.add_argument(
        "--catalog-id",
        default=DEFAULT_CATALOG_ID,
        help="Catalog ID to use in the createSurface message.",
    )
    parser.add_argument(
        "--hours",
        type=int,
        default=6,
        help="Number of hourly rows to render in weather mode.",
    )
    parser.add_argument(
        "--message",
        help="Optional override for loading, empty, or error detail text.",
    )
    parser.add_argument(
        "--latitude",
        type=float,
        default=37.5665,
        help="Latitude used when --source open-meteo is selected.",
    )
    parser.add_argument(
        "--longitude",
        type=float,
        default=126.9780,
        help="Longitude used when --source open-meteo is selected.",
    )
    parser.add_argument(
        "--city",
        default="서울",
        help="City label used when --source open-meteo is selected.",
    )
    parser.add_argument(
        "--district",
        default="중구",
        help="District label used when --source open-meteo is selected.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized weather payload before A2UI generation.",
    )
    return parser.parse_args()


def clean_text(value: Any, *, max_len: int = 80) -> str:
    text = " ".join(str(value or "").split())
    if not text:
        return ""
    if len(text) <= max_len:
        return text
    return f"{text[: max_len - 3].rstrip()}..."


def format_temperature(value: Any) -> str:
    try:
        return f"{int(round(float(value)))}°"
    except (TypeError, ValueError):
        return "-"


def format_percent(value: Any) -> str:
    try:
        return f"{int(round(float(value)))}%"
    except (TypeError, ValueError):
        return "-"


def parse_iso8601(value: str | None) -> datetime | None:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value)
    except ValueError:
        return None


def format_updated_at(value: str | None) -> str:
    dt = parse_iso8601(value)
    if dt is None:
        return "업데이트 시각 확인 필요"
    meridiem = "오전" if dt.hour < 12 else "오후"
    hour = dt.hour % 12 or 12
    return f"{meridiem} {hour}:{dt.minute:02d} 기준"


def format_hour_label(hour: dict[str, Any]) -> str:
    if hour.get("label"):
        return clean_text(hour["label"], max_len=12)
    dt = parse_iso8601(hour.get("time"))
    if dt is None:
        return "-"
    return f"{dt.hour:02d}시"


def icon_name_for_condition(condition: str) -> str:
    normalized = clean_text(condition, max_len=32).lower()
    if any(token in normalized for token in ("rain", "showers", "비", "소나기")):
        return "rainy"
    if any(token in normalized for token in ("snow", "눈", "sleet")):
        return "acUnit"
    if any(token in normalized for token in ("storm", "thunder", "번개", "뇌우")):
        return "flashOn"
    if any(token in normalized for token in ("wind", "gust", "바람", "강풍")):
        return "air"
    if any(token in normalized for token in ("cloud", "흐림", "구름")):
        return "cloud"
    if any(token in normalized for token in ("clear", "sun", "맑음", "sunny")):
        return "wbSunny"
    return "cloud"


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError("Weather input JSON must be an object.")
    return data


def open_meteo_condition(code: Any) -> str:
    mapping = {
        0: "맑음",
        1: "대체로 맑음",
        2: "구름 조금",
        3: "흐림",
        45: "안개",
        48: "짙은 안개",
        51: "약한 이슬비",
        53: "이슬비",
        55: "강한 이슬비",
        56: "약한 어는 이슬비",
        57: "강한 어는 이슬비",
        61: "약한 비",
        63: "비",
        65: "강한 비",
        66: "약한 어는 비",
        67: "강한 어는 비",
        71: "약한 눈",
        73: "눈",
        75: "강한 눈",
        77: "눈날림",
        80: "약한 소나기",
        81: "소나기",
        82: "강한 소나기",
        85: "약한 눈 소나기",
        86: "강한 눈 소나기",
        95: "뇌우",
        96: "우박 가능 뇌우",
        99: "강한 우박 가능 뇌우",
    }
    try:
        return mapping.get(int(code), "날씨 정보")
    except (TypeError, ValueError):
        return "날씨 정보"


def fetch_open_meteo_payload(
    *,
    latitude: float,
    longitude: float,
    city: str,
    district: str,
    hours: int,
) -> dict[str, Any]:
    params = {
        "latitude": latitude,
        "longitude": longitude,
        "timezone": "Asia/Seoul",
        "current": ",".join(
            [
                "temperature_2m",
                "apparent_temperature",
                "relative_humidity_2m",
                "precipitation",
                "weather_code",
            ]
        ),
        "hourly": ",".join(
            [
                "temperature_2m",
                "precipitation_probability",
                "weather_code",
            ]
        ),
        "forecast_hours": max(1, min(hours, 24)),
    }
    url = f"https://api.open-meteo.com/v1/forecast?{urlencode(params)}"
    try:
        response = subprocess.run(
            [
                "curl",
                "-L",
                "--fail",
                "--silent",
                "--show-error",
                "--connect-timeout",
                "5",
                "--max-time",
                "20",
                url,
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("curl is required for live Open-Meteo fetches.") from exc
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(exc.stderr.strip() or "Open-Meteo fetch failed.") from exc

    data = json.loads(response.stdout)
    current = data.get("current", {})
    hourly = data.get("hourly", {})
    hourly_times = list(hourly.get("time", []))
    hourly_temps = list(hourly.get("temperature_2m", []))
    hourly_precips = list(hourly.get("precipitation_probability", []))
    hourly_codes = list(hourly.get("weather_code", []))
    normalized_hours = []
    for index, time_value in enumerate(hourly_times[:hours]):
        normalized_hours.append(
            {
                "time": time_value,
                "temperature_c": hourly_temps[index] if index < len(hourly_temps) else None,
                "precip_probability_pct": (
                    hourly_precips[index] if index < len(hourly_precips) else None
                ),
                "condition": open_meteo_condition(
                    hourly_codes[index] if index < len(hourly_codes) else None
                ),
            }
        )

    first_precip_probability = (
        normalized_hours[0].get("precip_probability_pct") if normalized_hours else None
    )
    current_code = current.get("weather_code")
    current_condition = open_meteo_condition(current_code)
    headline = (
        f"{city} 현재 {current_condition}, "
        f"체감 {format_temperature(current.get('apparent_temperature'))}입니다."
    )

    return {
        "location": {
            "city": clean_text(city, max_len=16),
            "district": clean_text(district, max_len=16),
        },
        "updated_at": current.get("time"),
        "headline": clean_text(headline, max_len=48),
        "current": {
            "temperature_c": current.get("temperature_2m"),
            "feels_like_c": current.get("apparent_temperature"),
            "condition": current_condition,
            "humidity_pct": current.get("relative_humidity_2m"),
            "precip_probability_pct": first_precip_probability,
        },
        "alert": {
            "level": "안내",
            "title": "공식 특보 연동 필요",
            "summary": "현재 화면은 Open-Meteo 실황과 예보를 사용합니다. 재난성 특보는 기상청 공식 채널과 별도로 연동하세요.",
            "source": "Open-Meteo",
            "issued_at": current.get("time"),
        },
        "hourly": normalized_hours,
        "footer": "실황과 예보는 Open-Meteo 기반이며, 특보는 공식 소스를 별도로 연결하는 편이 안전합니다.",
    }


def build_create_surface(surface_id: str, catalog_id: str) -> dict[str, Any]:
    return {
        "version": "v0.9",
        "createSurface": {
            "surfaceId": surface_id,
            "catalogId": catalog_id,
        },
    }


def build_status_messages(
    *,
    surface_id: str,
    catalog_id: str,
    title: str,
    detail: str,
    hint: str,
    button_label: str | None = None,
    button_event: str | None = None,
) -> list[dict[str, Any]]:
    components: list[dict[str, Any]] = [
        {
            "id": "root",
            "component": "Column",
            "children": ["statusCard"],
            "justify": "center",
            "align": "stretch",
        },
        {
            "id": "statusCard",
            "component": "Card",
            "child": "statusColumn",
        },
        {
            "id": "statusColumn",
            "component": "Column",
            "children": ["statusTitle", "statusDetail", "statusHint"],
        },
        {
            "id": "statusTitle",
            "component": "Text",
            "variant": "h1",
            "text": {"path": "/title"},
        },
        {
            "id": "statusDetail",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/detail"},
        },
        {
            "id": "statusHint",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/hint"},
        },
    ]

    if button_label and button_event:
        components.extend(
            [
                {
                    "id": "retryButton",
                    "component": "Button",
                    "variant": "primary",
                    "child": "retryButtonText",
                    "action": {"event": {"name": button_event}},
                },
                {
                    "id": "retryButtonText",
                    "component": "Text",
                    "text": clean_text(button_label, max_len=20),
                },
            ]
        )
        status_column = next(
            component for component in components if component["id"] == "statusColumn"
        )
        status_column["children"].append("retryButton")

    return [
        build_create_surface(surface_id, catalog_id),
        {
            "version": "v0.9",
            "updateDataModel": {
                "surfaceId": surface_id,
                "value": {
                    "title": clean_text(title, max_len=24),
                    "detail": clean_text(detail, max_len=80),
                    "hint": clean_text(hint, max_len=100),
                },
            },
        },
        {
            "version": "v0.9",
            "updateComponents": {
                "surfaceId": surface_id,
                "components": components,
            },
        },
    ]


def build_weather_messages(
    *,
    surface_id: str,
    catalog_id: str,
    payload: dict[str, Any],
    hours: int,
) -> list[dict[str, Any]]:
    location = payload.get("location", {})
    current = payload.get("current", {})
    alert = payload.get("alert", {})
    hourly = payload.get("hourly", [])
    if not isinstance(location, dict):
        location = {}
    if not isinstance(current, dict):
        current = {}
    if not isinstance(alert, dict):
        alert = {}
    if not isinstance(hourly, list):
        hourly = []

    city = clean_text(location.get("city") or "서울", max_len=16)
    district = clean_text(location.get("district"), max_len=16)
    title = city if not district else f"{city} {district}"
    headline = clean_text(
        payload.get("headline") or "지금 필요한 날씨만 크게 보여줍니다.", max_len=48
    )
    condition = clean_text(current.get("condition") or "정보 없음", max_len=16)
    current_temp = format_temperature(current.get("temperature_c"))
    feels_like = f"체감 {format_temperature(current.get('feels_like_c'))}"
    precipitation = f"강수확률 {format_percent(current.get('precip_probability_pct'))}"
    humidity = f"습도 {format_percent(current.get('humidity_pct'))}"
    updated_at = format_updated_at(payload.get("updated_at"))
    footer = clean_text(
        payload.get("footer") or "특보와 갱신 시각을 함께 표시합니다.", max_len=60
    )

    alert_title = clean_text(alert.get("title") or "현재 특보 없음", max_len=28)
    alert_summary = clean_text(
        alert.get("summary") or "공식 특보가 없으면 일반 참고 정보만 표시합니다.",
        max_len=96,
    )
    alert_source = clean_text(alert.get("source") or "정보 출처 확인 필요", max_len=24)
    alert_updated = format_updated_at(alert.get("issued_at") or payload.get("updated_at"))
    alert_icon = icon_name_for_condition(
        f"{alert.get('level', '')} {alert.get('title', '')}".strip()
    )
    current_icon = icon_name_for_condition(condition)

    component_list: list[dict[str, Any]] = [
        {
            "id": "root",
            "component": "Column",
            "children": [
                "heroCard",
                "summaryCard",
                "hourlyCard",
                "alertCard",
                "footerText",
            ],
        },
        {"id": "heroCard", "component": "Card", "child": "heroColumn"},
        {
            "id": "heroColumn",
            "component": "Column",
            "children": [
                "locationText",
                "headlineText",
                "currentRow",
                "updatedText",
            ],
        },
        {
            "id": "locationText",
            "component": "Text",
            "variant": "h1",
            "text": {"path": "/title"},
        },
        {
            "id": "headlineText",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/headline"},
        },
        {
            "id": "currentRow",
            "component": "Row",
            "justify": "spaceBetween",
            "align": "center",
            "children": ["temperatureText", "conditionColumn"],
        },
        {
            "id": "temperatureText",
            "component": "Text",
            "variant": "h2",
            "text": {"path": "/currentTemp"},
        },
        {
            "id": "conditionColumn",
            "component": "Column",
            "children": ["conditionRow", "feelsLikeText", "precipText"],
        },
        {
            "id": "conditionRow",
            "component": "Row",
            "children": ["conditionIcon", "conditionText"],
            "align": "center",
        },
        {
            "id": "conditionIcon",
            "component": "Icon",
            "name": current_icon,
        },
        {
            "id": "conditionText",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/condition"},
        },
        {
            "id": "feelsLikeText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/feelsLike"},
        },
        {
            "id": "precipText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/precipitation"},
        },
        {
            "id": "updatedText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/updatedAt"},
        },
        {"id": "summaryCard", "component": "Card", "child": "summaryRow"},
        {
            "id": "summaryRow",
            "component": "Row",
            "justify": "spaceBetween",
            "children": ["summaryFeels", "summaryHumidity", "summaryRain"],
        },
        {
            "id": "summaryFeels",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/feelsLike"},
        },
        {
            "id": "summaryHumidity",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/humidity"},
        },
        {
            "id": "summaryRain",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/precipitation"},
        },
        {"id": "hourlyCard", "component": "Card", "child": "hourlyColumn"},
        {
            "id": "hourlyColumn",
            "component": "Column",
            "children": ["hourlyTitle"],
        },
        {
            "id": "hourlyTitle",
            "component": "Text",
            "variant": "h3",
            "text": "오늘 시간대별",
        },
        {"id": "alertCard", "component": "Card", "child": "alertColumn"},
        {
            "id": "alertColumn",
            "component": "Column",
            "children": [
                "alertHeaderRow",
                "alertSummaryText",
                "alertMetaText",
            ],
        },
        {
            "id": "alertHeaderRow",
            "component": "Row",
            "children": ["alertIcon", "alertTitleText"],
            "align": "center",
        },
        {
            "id": "alertIcon",
            "component": "Icon",
            "name": alert_icon,
        },
        {
            "id": "alertTitleText",
            "component": "Text",
            "variant": "h4",
            "text": {"path": "/alertTitle"},
        },
        {
            "id": "alertSummaryText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/alertSummary"},
        },
        {
            "id": "alertMetaText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/alertMeta"},
        },
        {
            "id": "footerText",
            "component": "Text",
            "variant": "body",
            "text": {"path": "/footer"},
        },
    ]

    hourly_children = ["hourlyTitle"]
    hourly_data: dict[str, Any] = {}
    for index, hour in enumerate(hourly[: max(1, hours)], start=1):
        if not isinstance(hour, dict):
            continue
        time_key = f"hour{index}Time"
        temp_key = f"hour{index}Temp"
        rain_key = f"hour{index}Rain"
        row_id = f"hour{index}Row"
        icon_id = f"hour{index}Icon"
        time_id = f"hour{index}TimeText"
        rain_id = f"hour{index}RainText"
        temp_id = f"hour{index}TempText"
        hourly_data[time_key] = format_hour_label(hour)
        hourly_data[temp_key] = format_temperature(hour.get("temperature_c"))
        hourly_data[rain_key] = format_percent(hour.get("precip_probability_pct"))
        component_list.extend(
            [
                {
                    "id": row_id,
                    "component": "Row",
                    "justify": "spaceBetween",
                    "children": [time_id, icon_id, rain_id, temp_id],
                    "align": "center",
                },
                {
                    "id": time_id,
                    "component": "Text",
                    "variant": "h4",
                    "text": {"path": f"/{time_key}"},
                },
                {
                    "id": icon_id,
                    "component": "Icon",
                    "name": icon_name_for_condition(hour.get("condition", "")),
                },
                {
                    "id": rain_id,
                    "component": "Text",
                    "variant": "h4",
                    "text": {"path": f"/{rain_key}"},
                },
                {
                    "id": temp_id,
                    "component": "Text",
                    "variant": "h4",
                    "text": {"path": f"/{temp_key}"},
                },
            ]
        )
        hourly_children.append(row_id)

    hourly_column = next(
        component for component in component_list if component["id"] == "hourlyColumn"
    )
    hourly_column["children"] = hourly_children

    data_model: dict[str, Any] = {
        "title": title,
        "headline": headline,
        "currentTemp": current_temp,
        "condition": condition,
        "feelsLike": feels_like,
        "precipitation": precipitation,
        "humidity": humidity,
        "updatedAt": updated_at,
        "alertTitle": alert_title,
        "alertSummary": alert_summary,
        "alertMeta": f"{alert_source} · {alert_updated}",
        "footer": footer,
    }
    data_model.update(hourly_data)

    return [
        build_create_surface(surface_id, catalog_id),
        {
            "version": "v0.9",
            "updateDataModel": {
                "surfaceId": surface_id,
                "value": data_model,
            },
        },
        {
            "version": "v0.9",
            "updateComponents": {
                "surfaceId": surface_id,
                "components": component_list,
            },
        },
    ]


def emit(messages: list[dict[str, Any]]) -> None:
    for message in messages:
        sys.stdout.write(json.dumps(message, ensure_ascii=False))
        sys.stdout.write("\n")


def main() -> int:
    args = parse_args()

    if args.state == "loading":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title="날씨 준비 중",
                detail=args.message or "서울 날씨 정보를 가져오고 있습니다.",
                hint="현재 기온, 강수확률, 특보를 순서대로 채웁니다.",
            )
        )
        return 0

    if args.state == "empty":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title="표시할 날씨 없음",
                detail=args.message or "선택한 지역의 날씨 데이터가 아직 없습니다.",
                hint="지역을 다시 확인하거나 mock 데이터를 먼저 연결합니다.",
            )
        )
        return 0

    if args.state == "error":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title="날씨를 불러오지 못했습니다",
                detail=args.message or "네트워크 또는 데이터 소스 상태를 확인해 주세요.",
                hint="마지막 갱신 시각과 공식 특보 소스 연결 상태를 함께 점검합니다.",
                button_label="다시 시도",
                button_event="refreshWeather",
            )
        )
        return 0

    try:
        if args.source == "open-meteo":
            payload = fetch_open_meteo_payload(
                latitude=args.latitude,
                longitude=args.longitude,
                city=args.city,
                district=args.district,
                hours=max(1, min(args.hours, 6)),
            )
        else:
            payload = load_json(args.input)
    except Exception as exc:
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title="날씨를 불러오지 못했습니다",
                detail=clean_text(str(exc), max_len=88) or "네트워크 또는 데이터 소스 상태를 확인해 주세요.",
                hint="마지막 갱신 시각과 공식 특보 소스 연결 상태를 함께 점검합니다.",
                button_label="다시 시도",
                button_event="refreshWeather",
            )
        )
        return 0

    if args.dump_normalized:
        args.dump_normalized.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
    emit(
        build_weather_messages(
            surface_id=args.surface_id,
            catalog_id=args.catalog_id,
            payload=payload,
            hours=max(1, min(args.hours, 6)),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
