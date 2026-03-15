#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_CATALOG_ID = "https://a2ui.org/specification/v0_9/standard_catalog.json"


@dataclass(frozen=True)
class ScenarioSpec:
    skill_name: str
    title: str
    default_input: Path
    default_surface_id: str
    loading_title: str
    loading_detail: str
    loading_hint: str
    empty_title: str
    empty_detail: str
    empty_hint: str
    error_title: str
    error_detail: str
    error_hint: str
    retry_event: str


def parse_args(spec: ScenarioSpec) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=f"Generate A2UI messages for {spec.skill_name}."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=spec.default_input,
        help="Path to a normalized scenario JSON file used for --state scenario.",
    )
    parser.add_argument(
        "--surface-id",
        default=spec.default_surface_id,
        help="Surface ID to use in emitted A2UI messages.",
    )
    parser.add_argument(
        "--catalog-id",
        default=DEFAULT_CATALOG_ID,
        help="Catalog ID to use in the createSurface message.",
    )
    parser.add_argument(
        "--message",
        help="Optional override for loading, empty, or error detail text.",
    )
    return parser.parse_args()


def clean_text(value: Any, *, max_len: int = 80) -> str:
    text = " ".join(str(value or "").split())
    if not text:
        return ""
    if len(text) <= max_len:
        return text
    return f"{text[: max_len - 3].rstrip()}..."


def safe_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def load_payload(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, dict):
        raise ValueError("Scenario input JSON must be an object.")
    return payload


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
        {"id": "statusCard", "component": "Card", "child": "statusColumn"},
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
                    "title": clean_text(title, max_len=28),
                    "detail": clean_text(detail, max_len=88),
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


def build_scenario_messages(
    *,
    surface_id: str,
    catalog_id: str,
    spec: ScenarioSpec,
    payload: dict[str, Any],
) -> list[dict[str, Any]]:
    title = clean_text(payload.get("title") or spec.title, max_len=32)
    headline = clean_text(
        payload.get("headline") or "거실에서 빠르게 이해할 수 있는 핵심 정보만 표시합니다.",
        max_len=88,
    )
    footer = clean_text(payload.get("footer"), max_len=100)
    metrics = [metric for metric in safe_list(payload.get("primaryMetrics")) if isinstance(metric, dict)][:3]
    sections = [section for section in safe_list(payload.get("sections")) if isinstance(section, dict)][:4]
    alert = payload.get("alert") if isinstance(payload.get("alert"), dict) else {}
    actions = [action for action in safe_list(payload.get("actions")) if isinstance(action, dict)][:3]

    data_model: dict[str, Any] = {
        "title": title,
        "headline": headline,
        "footer": footer,
    }

    components: list[dict[str, Any]] = [
        {
            "id": "root",
            "component": "Column",
            "children": ["heroCard"],
        },
        {"id": "heroCard", "component": "Card", "child": "heroColumn"},
        {
            "id": "heroColumn",
            "component": "Column",
            "children": ["titleText", "headlineText"],
        },
        {
            "id": "titleText",
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
    ]

    root = components[0]

    if metrics:
        root["children"].append("metricsCard")
        metric_columns: list[str] = []
        components.extend(
            [
                {"id": "metricsCard", "component": "Card", "child": "metricsRow"},
                {
                    "id": "metricsRow",
                    "component": "Row",
                    "justify": "spaceBetween",
                    "children": metric_columns,
                },
            ]
        )
        for index, metric in enumerate(metrics, start=1):
            column_id = f"metric{index}Column"
            label_id = f"metric{index}Label"
            value_id = f"metric{index}Value"
            detail_id = f"metric{index}Detail"
            metric_columns.append(column_id)
            data_model[f"metric{index}Label"] = clean_text(metric.get("label"), max_len=24)
            data_model[f"metric{index}Value"] = clean_text(metric.get("value"), max_len=28)
            detail = clean_text(metric.get("detail"), max_len=40)
            column_children = [label_id, value_id]
            components.extend(
                [
                    {
                        "id": column_id,
                        "component": "Column",
                        "children": column_children,
                    },
                    {
                        "id": label_id,
                        "component": "Text",
                        "variant": "body",
                        "text": {"path": f"/metric{index}Label"},
                    },
                    {
                        "id": value_id,
                        "component": "Text",
                        "variant": "h3",
                        "text": {"path": f"/metric{index}Value"},
                    },
                ]
            )
            if detail:
                data_model[f"metric{index}Detail"] = detail
                column_children.append(detail_id)
                components.append(
                    {
                        "id": detail_id,
                        "component": "Text",
                        "variant": "body",
                        "text": {"path": f"/metric{index}Detail"},
                    }
                )

    for section_index, section in enumerate(sections, start=1):
        section_card_id = f"section{section_index}Card"
        section_column_id = f"section{section_index}Column"
        section_title_id = f"section{section_index}Title"
        section_children = [section_title_id]
        root["children"].append(section_card_id)
        data_model[f"section{section_index}Title"] = clean_text(
            section.get("title"), max_len=28
        )
        components.extend(
            [
                {
                    "id": section_card_id,
                    "component": "Card",
                    "child": section_column_id,
                },
                {
                    "id": section_column_id,
                    "component": "Column",
                    "children": section_children,
                },
                {
                    "id": section_title_id,
                    "component": "Text",
                    "variant": "h3",
                    "text": {"path": f"/section{section_index}Title"},
                },
            ]
        )

        items = [item for item in safe_list(section.get("items")) if isinstance(item, dict)][:5]
        for item_index, item in enumerate(items, start=1):
            row_id = f"section{section_index}Item{item_index}Row"
            label_id = f"section{section_index}Item{item_index}Label"
            value_id = f"section{section_index}Item{item_index}Value"
            detail_id = f"section{section_index}Item{item_index}Detail"
            icon_id = f"section{section_index}Item{item_index}Icon"
            row_children = []
            icon_name = clean_text(item.get("icon"), max_len=24)
            if icon_name:
                row_children.append(icon_id)
                components.append(
                    {
                        "id": icon_id,
                        "component": "Icon",
                        "name": icon_name,
                    }
                )
            row_children.extend([label_id, value_id])
            section_children.append(row_id)
            data_model[f"section{section_index}Item{item_index}Label"] = clean_text(
                item.get("label"), max_len=40
            )
            data_model[f"section{section_index}Item{item_index}Value"] = clean_text(
                item.get("value"), max_len=32
            )
            components.extend(
                [
                    {
                        "id": row_id,
                        "component": "Row",
                        "justify": "spaceBetween",
                        "align": "center",
                        "children": row_children,
                    },
                    {
                        "id": label_id,
                        "component": "Text",
                        "variant": "h4",
                        "text": {"path": f"/section{section_index}Item{item_index}Label"},
                    },
                    {
                        "id": value_id,
                        "component": "Text",
                        "variant": "h4",
                        "text": {"path": f"/section{section_index}Item{item_index}Value"},
                    },
                ]
            )
            detail = clean_text(item.get("detail"), max_len=80)
            if detail:
                data_model[f"section{section_index}Item{item_index}Detail"] = detail
                section_children.append(detail_id)
                components.append(
                    {
                        "id": detail_id,
                        "component": "Text",
                        "variant": "body",
                        "text": {"path": f"/section{section_index}Item{item_index}Detail"},
                    }
                )

    alert_title = clean_text(alert.get("title"), max_len=28)
    alert_summary = clean_text(alert.get("summary"), max_len=100)
    alert_meta = clean_text(alert.get("meta"), max_len=60)
    alert_icon = clean_text(alert.get("icon"), max_len=24) or "info"
    if alert_title or alert_summary:
        root["children"].append("alertCard")
        data_model["alertTitle"] = alert_title or "안내"
        data_model["alertSummary"] = alert_summary or "세부 안내를 확인하세요."
        data_model["alertMeta"] = alert_meta
        components.extend(
            [
                {"id": "alertCard", "component": "Card", "child": "alertColumn"},
                {
                    "id": "alertColumn",
                    "component": "Column",
                    "children": ["alertHeaderRow", "alertSummaryText"],
                },
                {
                    "id": "alertHeaderRow",
                    "component": "Row",
                    "children": ["alertIcon", "alertTitleText"],
                    "align": "center",
                },
                {"id": "alertIcon", "component": "Icon", "name": alert_icon},
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
            ]
        )
        if alert_meta:
            alert_column = next(
                component for component in components if component["id"] == "alertColumn"
            )
            alert_column["children"].append("alertMetaText")
            components.append(
                {
                    "id": "alertMetaText",
                    "component": "Text",
                    "variant": "body",
                    "text": {"path": "/alertMeta"},
                }
            )

    if actions:
        root["children"].append("actionsCard")
        button_ids: list[str] = []
        components.extend(
            [
                {"id": "actionsCard", "component": "Card", "child": "actionsRow"},
                {
                    "id": "actionsRow",
                    "component": "Row",
                    "justify": "spaceAround",
                    "children": button_ids,
                },
            ]
        )
        for index, action in enumerate(actions, start=1):
            button_id = f"action{index}Button"
            text_id = f"action{index}ButtonText"
            button_ids.append(button_id)
            event_name = clean_text(action.get("event"), max_len=32) or spec.retry_event
            components.extend(
                [
                    {
                        "id": button_id,
                        "component": "Button",
                        "child": text_id,
                        "action": {"event": {"name": event_name}},
                    },
                    {
                        "id": text_id,
                        "component": "Text",
                        "text": clean_text(action.get("label"), max_len=20) or "열기",
                    },
                ]
            )
            if index == 1:
                components[-2]["variant"] = "primary"

    if footer:
        root["children"].append("footerText")
        components.append(
            {
                "id": "footerText",
                "component": "Text",
                "variant": "body",
                "text": {"path": "/footer"},
            }
        )

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
                "components": components,
            },
        },
    ]


def emit(messages: list[dict[str, Any]]) -> None:
    for message in messages:
        sys.stdout.write(json.dumps(message, ensure_ascii=False))
        sys.stdout.write("\n")


def main(spec: ScenarioSpec) -> int:
    args = parse_args(spec)

    if args.state == "loading":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=spec.loading_title,
                detail=args.message or spec.loading_detail,
                hint=spec.loading_hint,
            )
        )
        return 0

    if args.state == "empty":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=spec.empty_title,
                detail=args.message or spec.empty_detail,
                hint=spec.empty_hint,
            )
        )
        return 0

    if args.state == "error":
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=spec.error_title,
                detail=args.message or spec.error_detail,
                hint=spec.error_hint,
                button_label="다시 시도",
                button_event=spec.retry_event,
            )
        )
        return 0

    try:
        payload = load_payload(args.input)
    except Exception as exc:
        emit(
            build_status_messages(
                surface_id=args.surface_id,
                catalog_id=args.catalog_id,
                title=spec.error_title,
                detail=clean_text(str(exc), max_len=88) or spec.error_detail,
                hint=spec.error_hint,
                button_label="다시 시도",
                button_event=spec.retry_event,
            )
        )
        return 0
    emit(
        build_scenario_messages(
            surface_id=args.surface_id,
            catalog_id=args.catalog_id,
            spec=spec,
            payload=payload,
        )
    )
    return 0
