#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import format_korean_datetime, parse_rfc822, fetch_text
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


DEFAULT_RSS_URL = "https://www.yonhapnewstv.co.kr/browse/feed/"

SPEC = ScenarioSpec(
    skill_name="tv-news-briefing",
    title="TV News Briefing",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="news_main",
    loading_title="뉴스 준비 중",
    loading_detail="주요 헤드라인과 카테고리별 뉴스를 정리하고 있습니다.",
    loading_hint="리드 스토리부터 표시한 뒤 나머지 헤드라인을 채웁니다.",
    empty_title="표시할 뉴스 없음",
    empty_detail="현재 보여줄 뉴스 카드가 아직 준비되지 않았습니다.",
    empty_hint="피드 연결 상태와 카테고리 매핑을 먼저 확인하세요.",
    error_title="뉴스를 불러오지 못했습니다",
    error_detail="피드 연결 또는 요약 파이프라인 상태를 확인해 주세요.",
    error_hint="출처 표기와 마지막 갱신 시각이 빠지지 않았는지도 함께 확인합니다.",
    retry_event="refreshNews",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV news briefing skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "yonhap-rss"],
        default="mock",
        help="Choose whether to use the bundled mock file or the live Yonhap News TV RSS feed.",
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
        "--rss-url",
        default=DEFAULT_RSS_URL,
        help="RSS URL used when --source yonhap-rss is selected.",
    )
    parser.add_argument(
        "--count",
        type=int,
        default=6,
        help="Number of feed items to include from the live RSS feed.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized news payload before A2UI generation.",
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


def safe_dt(value: str | None) -> datetime | None:
    if not value:
        return None
    try:
        return parse_rfc822(value)
    except (TypeError, ValueError, IndexError):
        return None


def normalize_feed_item(item: ET.Element) -> dict[str, str]:
    title = clean_text(item.findtext("title"), max_len=88)
    link = clean_text(item.findtext("link"), max_len=120)
    category = clean_text(item.findtext("category") or "최신", max_len=20)
    pub_date_raw = item.findtext("pubDate")
    pub_date = safe_dt(pub_date_raw)
    label = "속보" if "[속보]" in title else category
    detail = "시각 확인 필요"
    if pub_date is not None:
        detail = f"{format_korean_datetime(pub_date)} · 연합뉴스TV"
    return {
        "title": title,
        "link": link,
        "label": label,
        "detail": detail,
        "is_breaking": "1" if "[속보]" in title else "0",
    }


def normalize_yonhap_rss(xml_text: str, count: int) -> dict[str, object]:
    root = ET.fromstring(xml_text)
    channel = root.find("channel")
    if channel is None:
        raise RuntimeError("RSS channel not found.")

    publisher = "연합뉴스TV"
    last_build = safe_dt(channel.findtext("lastBuildDate"))
    items = [normalize_feed_item(item) for item in channel.findall("item")[: max(1, count)]]
    if not items:
        raise RuntimeError("No RSS items found.")

    lead = items[0]
    secondary = items[1:]
    breaking_count = sum(1 for item in items if item["is_breaking"] == "1")

    sections = [
        {
            "title": "리드 스토리",
            "items": [
                {
                    "icon": "article",
                    "label": lead["label"],
                    "value": lead["title"],
                    "detail": lead["detail"],
                }
            ],
        }
    ]

    if secondary:
        sections.append(
            {
                "title": "최신 헤드라인",
                "items": [
                    {
                        "icon": "article",
                        "label": item["label"],
                        "value": item["title"],
                        "detail": item["detail"],
                    }
                    for item in secondary[:5]
                ],
            }
        )

    updated_label = "갱신 시각 확인 필요"
    if last_build is not None:
        updated_label = f"{format_korean_datetime(last_build)} 기준"

    payload = {
        "title": "오늘의 주요 뉴스",
        "headline": lead["title"],
        "primaryMetrics": [
            {"label": "출처", "value": publisher, "detail": updated_label},
            {"label": "헤드라인", "value": f"{len(items)}건", "detail": "최신 RSS 피드"},
            {
                "label": "속보",
                "value": f"{breaking_count}건",
                "detail": "제목 기준 속보 표기",
            },
        ],
        "sections": sections,
        "alert": {
            "icon": "info",
            "title": "출처와 시각 유지",
            "summary": "라이브 뉴스 화면은 기사 원문 링크를 보존하고 발행 또는 갱신 시각을 함께 표시하는 편이 안전합니다.",
            "meta": publisher,
        },
        "actions": [
            {"label": "새로고침", "event": "refreshNews"},
            {"label": "리드 보기", "event": "openLeadStory"},
        ],
        "footer": "요약 문장은 기사 본문을 과감하게 재서술하기보다 헤드라인과 시각 중심으로 짧게 유지합니다.",
    }
    return payload


def main() -> int:
    args = parse_args()
    status = emit_status(args)
    if status == 0:
        return 0

    try:
        if args.source == "yonhap-rss":
            payload = normalize_yonhap_rss(fetch_text(args.rss_url), args.count)
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
