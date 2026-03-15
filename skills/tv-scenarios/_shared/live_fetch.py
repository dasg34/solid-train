#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
from datetime import datetime
from email.utils import parsedate_to_datetime
from typing import Any
from zoneinfo import ZoneInfo


KST = ZoneInfo("Asia/Seoul")
CONNECT_TIMEOUT_SECONDS = 5
MAX_FETCH_SECONDS = 20


def fetch_text(url: str) -> str:
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
            text=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError("curl is required for live fetches.") from exc
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(exc.stderr.strip() or "Live fetch failed.") from exc
    return response.stdout


def fetch_json(url: str) -> dict[str, Any]:
    payload = json.loads(fetch_text(url))
    if not isinstance(payload, dict):
        raise RuntimeError("Live JSON response must be an object.")
    return payload


def to_kst(dt: datetime) -> datetime:
    if dt.tzinfo is None:
        return dt.replace(tzinfo=KST)
    return dt.astimezone(KST)


def format_korean_datetime(dt: datetime) -> str:
    dt = to_kst(dt)
    meridiem = "오전" if dt.hour < 12 else "오후"
    hour = dt.hour % 12 or 12
    return f"{meridiem} {hour}:{dt.minute:02d}"


def format_korean_date(dt: datetime) -> str:
    dt = to_kst(dt)
    return f"{dt.month}월 {dt.day}일"


def parse_rfc822(value: str) -> datetime:
    return to_kst(parsedate_to_datetime(value))
