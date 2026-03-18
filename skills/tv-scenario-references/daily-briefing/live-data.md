# TV Daily Briefing Live Data

## Default Compose Path

- `weather`: Open-Meteo forecast API
- `news`: Yonhap News TV RSS
- `schedule`: ICS URL or local ICS file
- `commute`: Nominatim geocoding + OSRM routing by default in `compose-live`
- Locale baseline: `ko-KR`
- Time zone baseline: `Asia/Seoul`

## Commands

Run from `tv-daily-briefing/`.

```bash
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live --schedule-source mock
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live --ics-url https://holidays.hyunbin.page/basic.ics
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live --ics-file /path/to/calendar.ics
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live --schedule-source mock --commute-origin "서울시청" --commute-destination "강남역"
python3 scripts/generate_daily_briefing_a2ui.py --source compose-live --commute-source skip
```

## Partial Fallback Policy

- Weather, news, schedule, and commute are fetched independently.
- If one source fails, keep the remaining cards visible.
- If the selected ICS window has no events, treat schedule as `empty` instead of failing the whole briefing.
- If every card fails or is skipped, emit the skill-level error state.

## Source Notes

- Open-Meteo is good for quick forecast and hourly trend cards, but not for official Korean severe alerts.
- Yonhap RSS is headline-first and works well for TV summary cards because it preserves source and publish timing without requiring full-article extraction.
- The sample Korea holiday ICS feed is useful for testing, but it can legitimately return no events for the current date window. Around `2026-03-15`, a short `2-day` or `30-day` window may be empty.
- For a fuller demo, keep weather and news live, and temporarily use `--schedule-source mock` until a real household or work calendar ICS is available.
- Commute is now part of the default compose path. If geocoding or routing connectivity is flaky in a demo environment, use `--commute-source mock` or `--commute-source skip` instead of disabling compose mode entirely.
- When the inferred arrival target is far away, the TV-safe summary should stay short instead of exposing minute-precision long waits.

## Output Shape

The compose adapter emits one normalized daily payload with:

- `primaryMetrics[]`
- `sections[]`
- `alert`
- `actions[]`
- `footer`

It then converts that payload into newline-delimited A2UI messages using the shared scenario renderer.
