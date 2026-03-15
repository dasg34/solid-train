#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any
from urllib.parse import quote

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_shared"))

from live_fetch import CONNECT_TIMEOUT_SECONDS, KST, MAX_FETCH_SECONDS, fetch_json
from scenario_a2ui import (
    ScenarioSpec,
    build_scenario_messages,
    build_status_messages,
    clean_text,
    emit,
    load_payload,
)


SPEC = ScenarioSpec(
    skill_name="tv-finance-snapshot",
    title="TV Finance Snapshot",
    default_input=Path(__file__).resolve().parents[1] / "references" / "mock_surface.json",
    default_surface_id="finance_main",
    loading_title="시장 정보 준비 중",
    loading_detail="국내 지수, 관심종목, 환율 정보를 정리하고 있습니다.",
    loading_hint="국내 시장과 KRW 기준 지표를 먼저 채웁니다.",
    empty_title="표시할 시장 정보 없음",
    empty_detail="연결된 watchlist 또는 환율 카드가 아직 없습니다.",
    empty_hint="watchlist 코드나 live source 설정을 먼저 확인해 주세요.",
    error_title="시장 정보를 불러오지 못했습니다",
    error_detail="시세 피드 또는 환율 API 상태를 확인해 주세요.",
    error_hint="투자 조언처럼 보이지 않도록 사실 기반 요약으로만 유지하는 편이 안전합니다.",
    retry_event="refreshFinance",
)

DEFAULT_WATCHLIST = "005930:삼성전자,000660:SK하이닉스,035420:NAVER"
NAVER_REALTIME_URL = "https://polling.finance.naver.com/api/realtime?query="
FRANKFURTER_URL = "https://api.frankfurter.dev/v1"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate A2UI messages for the TV finance snapshot skill."
    )
    parser.add_argument(
        "--state",
        choices=["scenario", "loading", "empty", "error"],
        default="scenario",
        help="Choose which UI state to emit.",
    )
    parser.add_argument(
        "--source",
        choices=["mock", "naver-public"],
        default="mock",
        help="Use the bundled mock payload or fetch market data from public Naver and Frankfurter sources.",
    )
    parser.add_argument(
        "--input",
        type=Path,
        default=SPEC.default_input,
        help="Path to a normalized finance JSON file used for --source mock.",
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
        "--watchlist",
        default=DEFAULT_WATCHLIST,
        help="Comma-separated domestic stock codes, optionally CODE:LABEL pairs.",
    )
    parser.add_argument(
        "--dump-normalized",
        type=Path,
        help="Optional path to save the normalized finance payload before A2UI generation.",
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


def parse_watchlist(value: str) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    for raw_entry in value.split(","):
        token = raw_entry.strip()
        if not token:
            continue
        if ":" in token:
            code, label = token.split(":", 1)
        else:
            code, label = token, token
        code = code.strip()
        label = clean_text(label.strip(), max_len=24) or code
        if len(code) != 6 or not code.isdigit():
            raise RuntimeError("watchlist entries must use 6-digit domestic stock codes.")
        entries.append({"code": code, "label": label})
    if not entries:
        raise RuntimeError("watchlist must contain at least one stock code.")
    return entries[:5]


def signed_number(value: Any, rf: str | None) -> float:
    try:
        amount = float(value)
    except (TypeError, ValueError):
        return 0.0
    if amount < 0:
        return amount
    if rf == "5":
        return -amount
    if rf == "2":
        return amount
    return 0.0 if amount == 0 else amount


def market_state_label(raw: Any) -> str:
    token = clean_text(raw, max_len=20).upper()
    if token == "CLOSE":
        return "장마감"
    if "OPEN" in token:
        return "장중"
    if "AFTER" in token:
        return "시간외"
    return "시장 상태 확인"


def format_number(value: float, *, decimals: int = 2) -> str:
    return f"{value:,.{decimals}f}"


def format_signed(value: float, *, decimals: int = 2, suffix: str = "") -> str:
    sign = "+" if value > 0 else ""
    return f"{sign}{value:,.{decimals}f}{suffix}"


def format_stock_price(value: float) -> str:
    return f"{value:,.0f}원"


def direction_icon(value: float) -> str:
    if value > 0:
        return "arrowUpward"
    if value < 0:
        return "arrowDownward"
    return "showChart"


def fetch_naver_json(url: str) -> dict[str, Any]:
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
                url,
            ],
            check=True,
            capture_output=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("curl is required for live finance fetches.") from exc
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(stderr or "Live finance fetch failed.") from exc

    for encoding in ("utf-8", "cp949", "euc-kr"):
        try:
            return json.loads(response.stdout.decode(encoding))
        except UnicodeDecodeError:
            continue
    raise RuntimeError("Naver finance response encoding is not supported.")


def fetch_naver_area(query: str) -> tuple[dict[str, Any], datetime]:
    payload = fetch_naver_json(f"{NAVER_REALTIME_URL}{quote(query, safe=':|')}")
    result = payload.get("result")
    if not isinstance(result, dict):
        raise RuntimeError("Naver polling response is missing result.")
    areas = result.get("areas")
    if not isinstance(areas, list) or not areas:
        raise RuntimeError("Naver polling response is missing areas.")
    first = areas[0]
    if not isinstance(first, dict):
        raise RuntimeError("Naver polling response area is invalid.")
    datas = first.get("datas")
    if not isinstance(datas, list) or not datas or not isinstance(datas[0], dict):
        raise RuntimeError("Naver polling response area is missing quote data.")
    timestamp = result.get("time")
    if isinstance(timestamp, (int, float)):
        fetched_at = datetime.fromtimestamp(float(timestamp) / 1000.0, tz=KST)
    else:
        fetched_at = datetime.now(KST)
    return datas[0], fetched_at


def fetch_index_snapshot(code: str) -> dict[str, Any]:
    data, fetched_at = fetch_naver_area(f"SERVICE_INDEX:{code}")
    current = float(data.get("nv", 0.0)) / 100.0
    change = signed_number(data.get("cv"), clean_text(data.get("rf"), max_len=4)) / 100.0
    change_pct = signed_number(data.get("cr"), clean_text(data.get("rf"), max_len=4))
    return {
        "code": code,
        "label": code,
        "current": current,
        "change": change,
        "change_pct": change_pct,
        "status": market_state_label(data.get("ms")),
        "fetched_at": fetched_at,
    }


def fetch_stock_snapshot(code: str, label: str) -> dict[str, Any]:
    data, fetched_at = fetch_naver_area(f"SERVICE_ITEM:{code}")
    change = signed_number(data.get("cv"), clean_text(data.get("rf"), max_len=4))
    change_pct = signed_number(data.get("cr"), clean_text(data.get("rf"), max_len=4))
    return {
        "code": code,
        "label": label,
        "current": float(data.get("nv", 0.0)),
        "change": change,
        "change_pct": change_pct,
        "status": market_state_label(data.get("ms")),
        "fetched_at": fetched_at,
    }


def fetch_exchange_snapshot() -> dict[str, Any]:
    latest = fetch_json(f"{FRANKFURTER_URL}/latest?base=KRW&symbols=USD,JPY")
    latest_date = latest.get("date")
    latest_rates = latest.get("rates")
    if not isinstance(latest_date, str) or not isinstance(latest_rates, dict):
        raise RuntimeError("Frankfurter latest response is missing rates.")
    if latest_rates.get("USD") in (None, 0) or latest_rates.get("JPY") in (None, 0):
        raise RuntimeError("Frankfurter latest response is missing KRW conversion rates.")

    start_date = (datetime.fromisoformat(latest_date) - timedelta(days=7)).date().isoformat()
    history = fetch_json(
        f"{FRANKFURTER_URL}/{start_date}..{latest_date}?base=KRW&symbols=USD,JPY"
    )
    rates_by_date = history.get("rates")
    if not isinstance(rates_by_date, dict):
        raise RuntimeError("Frankfurter history response is missing rates.")

    available_dates = sorted(
        date_key for date_key, value in rates_by_date.items() if isinstance(value, dict)
    )
    if not available_dates:
        raise RuntimeError("Frankfurter history response has no usable dates.")
    previous_date = available_dates[-2] if len(available_dates) >= 2 else available_dates[-1]
    previous_rates = rates_by_date.get(previous_date)
    if not isinstance(previous_rates, dict):
        raise RuntimeError("Frankfurter history response is missing a previous rate snapshot.")

    usd_latest = float(latest_rates["USD"])
    jpy_latest = float(latest_rates["JPY"])
    usd_previous = float(previous_rates["USD"])
    jpy_previous = float(previous_rates["JPY"])

    usd_krw = 1.0 / usd_latest
    usd_krw_prev = 1.0 / usd_previous
    jpy100_krw = 100.0 / jpy_latest
    jpy100_krw_prev = 100.0 / jpy_previous

    return {
        "date": latest_date,
        "usd_krw": usd_krw,
        "usd_krw_change": usd_krw - usd_krw_prev,
        "jpy100_krw": jpy100_krw,
        "jpy100_krw_change": jpy100_krw - jpy100_krw_prev,
    }


def strongest_mover(stocks: list[dict[str, Any]]) -> dict[str, Any] | None:
    if not stocks:
        return None
    return max(stocks, key=lambda item: abs(float(item.get("change_pct", 0.0))))


def finance_alert(
    *,
    errors: list[str],
    sources: list[str],
) -> dict[str, str]:
    if errors:
        return {
            "icon": "warning",
            "title": "일부 지표만 표시 중",
            "summary": clean_text(" / ".join(errors), max_len=100),
            "meta": clean_text(" · ".join(sources), max_len=48),
        }
    return {
        "icon": "gavel",
        "title": "투자 참고용 요약",
        "summary": "TV 금융 스냅샷은 참고용 정보로만 유지하고, 추천이나 매수 유도처럼 보이는 표현은 피하는 편이 안전합니다.",
        "meta": clean_text(" · ".join(sources), max_len=48),
    }


def build_live_payload(
    *,
    watchlist: list[dict[str, str]],
    kospi: dict[str, Any] | None,
    kosdaq: dict[str, Any] | None,
    fx: dict[str, Any] | None,
    stocks: list[dict[str, Any]],
    errors: list[str],
) -> dict[str, Any]:
    if not kospi and not fx and not stocks:
        raise RuntimeError("표시 가능한 live 금융 카드가 없습니다.")

    rising_count = sum(1 for stock in stocks if float(stock.get("change_pct", 0.0)) > 0)
    mover = strongest_mover(stocks)

    headline_parts: list[str] = []
    if kospi:
        headline_parts.append(
            clean_text(
                f"코스피 {format_number(float(kospi['current']))} ({format_signed(float(kospi['change_pct']), suffix='%' )})",
                max_len=34,
            )
        )
    if fx:
        headline_parts.append(
            clean_text(
                f"원/달러 {format_number(float(fx['usd_krw']))}원",
                max_len=24,
            )
        )
    if stocks:
        headline_parts.append(f"관심종목 {rising_count} / {len(stocks)} 상승")

    market_metric = {
        "label": "코스피",
        "value": format_number(float(kospi["current"])) if kospi else "-",
        "detail": (
            clean_text(
                f"{format_signed(float(kospi['change']), suffix='p')} · {format_signed(float(kospi['change_pct']), suffix='%')} · {kospi['status']}",
                max_len=36,
            )
            if kospi
            else "국내 지수 연결 필요"
        ),
    }
    fx_metric = {
        "label": "USD/KRW",
        "value": format_number(float(fx["usd_krw"])) if fx else "-",
        "detail": (
            clean_text(
                f"전일 대비 {format_signed(float(fx['usd_krw_change']))} · {fx['date']} 기준",
                max_len=40,
            )
            if fx
            else "환율 연결 필요"
        ),
    }
    mover_detail = "관심종목 연결 필요"
    if mover is not None:
        mover_detail = clean_text(
            f"최대 변동 {mover['label']} {format_signed(float(mover['change_pct']), suffix='%')}",
            max_len=36,
        )
    watchlist_metric = {
        "label": "관심종목",
        "value": f"{rising_count} / {len(stocks)} 상승" if stocks else "0 / 0 상승",
        "detail": mover_detail,
    }

    watch_items = [
        {
            "icon": direction_icon(float(stock["change_pct"])),
            "label": stock["label"],
            "value": format_signed(float(stock["change_pct"]), suffix="%"),
            "detail": clean_text(
                f"{format_stock_price(float(stock['current']))} · {stock['status']}",
                max_len=48,
            ),
        }
        for stock in stocks[:3]
    ]
    indicator_items: list[dict[str, str]] = []
    if kospi:
        indicator_items.append(
            {
                "icon": direction_icon(float(kospi["change_pct"])),
                "label": "코스피",
                "value": format_number(float(kospi["current"])),
                "detail": clean_text(
                    f"{format_signed(float(kospi['change_pct']), suffix='%')} · {kospi['status']}",
                    max_len=36,
                ),
            }
        )
    if kosdaq:
        indicator_items.append(
            {
                "icon": direction_icon(float(kosdaq["change_pct"])),
                "label": "코스닥",
                "value": format_number(float(kosdaq["current"])),
                "detail": clean_text(
                    f"{format_signed(float(kosdaq['change_pct']), suffix='%')} · {kosdaq['status']}",
                    max_len=36,
                ),
            }
        )
    if fx:
        indicator_items.append(
            {
                "icon": "currencyExchange",
                "label": "USD/KRW",
                "value": format_number(float(fx["usd_krw"])),
                "detail": clean_text(
                    f"전일 대비 {format_signed(float(fx['usd_krw_change']))} · {fx['date']}",
                    max_len=40,
                ),
            }
        )
        indicator_items.append(
            {
                "icon": "currencyExchange",
                "label": "100JPY/KRW",
                "value": format_number(float(fx["jpy100_krw"])),
                "detail": clean_text(
                    f"전일 대비 {format_signed(float(fx['jpy100_krw_change']))} · {fx['date']}",
                    max_len=40,
                ),
            }
        )

    requested_labels = ", ".join(entry["label"] for entry in watchlist)
    sources = ["Npay 증권 polling", "Frankfurter / ECB"]

    return {
        "title": "시장 스냅샷",
        "headline": clean_text(
            " · ".join(part for part in headline_parts if part)
            or "KRW 기준 환율과 관심종목 움직임을 짧게 정리했습니다.",
            max_len=88,
        ),
        "primaryMetrics": [market_metric, fx_metric, watchlist_metric],
        "sections": [
            {"title": "관심종목", "items": watch_items},
            {"title": "참고 지표", "items": indicator_items[:4]},
        ],
        "alert": finance_alert(errors=errors, sources=sources),
        "actions": [
            {"label": "새로고침", "event": "refreshFinance"},
            {"label": "환율 보기", "event": "showExchangeRates"},
        ],
        "footer": clean_text(
            f"출처: {', '.join(sources)}. 요청 watchlist: {requested_labels}. 장중/장마감 상태와 고시일을 함께 확인하는 편이 안전합니다.",
            max_len=100,
        ),
    }


def main() -> int:
    args = parse_args()
    status = emit_status(args, args.state)
    if status == 0:
        return 0

    try:
        if args.source == "naver-public":
            watchlist = parse_watchlist(args.watchlist)
            errors: list[str] = []

            try:
                kospi = fetch_index_snapshot("KOSPI")
            except Exception as exc:
                kospi = None
                errors.append(f"코스피 연결 실패: {clean_text(exc, max_len=34)}")

            try:
                kosdaq = fetch_index_snapshot("KOSDAQ")
            except Exception:
                kosdaq = None

            try:
                fx = fetch_exchange_snapshot()
            except Exception as exc:
                fx = None
                errors.append(f"환율 연결 실패: {clean_text(exc, max_len=34)}")

            stocks: list[dict[str, Any]] = []
            for entry in watchlist:
                try:
                    stocks.append(fetch_stock_snapshot(entry["code"], entry["label"]))
                except Exception as exc:
                    errors.append(
                        f"{entry['label']} 연결 실패: {clean_text(exc, max_len=28)}"
                    )

            if not stocks and not kospi and not fx:
                return emit_status(args, "error", detail="국내 지수, 환율, watchlist를 모두 불러오지 못했습니다.")

            payload = build_live_payload(
                watchlist=watchlist,
                kospi=kospi,
                kosdaq=kosdaq,
                fx=fx,
                stocks=stocks,
                errors=errors,
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
