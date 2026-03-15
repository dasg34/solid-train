---
name: tv-weather-briefing
description: "Plan, assess, and implement a Korea-first TV weather briefing surface for the Samsung Tizen GenUI PoC. Use when the user asks for weather, precipitation, feels-like temperature, alerts, or utterances such as `날씨 보여줘`."
---

# TV Weather Briefing

Use this skill to scope, design, implement, or simulate a TV-friendly weather surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_weather_a2ui.py
python3 scripts/generate_weather_a2ui.py --state loading
python3 scripts/generate_weather_a2ui.py --state error
python3 scripts/generate_weather_a2ui.py --source open-meteo --city 서울 --district 중구 --hours 4
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. Use the default mock payload in `references/mock_weather_seoul.json` first, then swap in live weather data without changing the overall surface contract.

## Workflow

1. Anchor the user intent around these requests: `날씨 보여줘`, `오늘 날씨 보여줘`, `비 와?`.
2. Choose the surface shape early. Default to `fullscreen scene` and optimize for 10-foot readability.
3. Reuse `scripts/generate_weather_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `current conditions`, `hourly trend`, `precipitation`, `feels-like`, `alerts`.
5. Start with `mock-first, live-later` and expand to live data only after the dependency list in `references/feasibility.md` is stable.
6. Include loading, empty, degraded, and error states before polishing animation or secondary controls.
7. Treat A2UI payloads, agent responses, and backend data as untrusted input. Keep alert freshness visible and frame severe weather as reference information unless the source is official.
8. Reuse local references:
   - `genui/examples/simple_chat` for minimal `SurfaceController` wiring
   - `genui/examples/travel_app` for domain-specific catalogs and tool-backed patterns
   - `genui/examples/verdure/client` for app shell, routing, and polished loading states
   - `genui/packages/genui_a2a` only when direct A2A transport is required

## Input Contract

Use `references/mock_weather_seoul.json` as the baseline schema.

- `location.city`, `location.district`
- `updated_at`
- `headline`
- `current.temperature_c`, `current.feels_like_c`, `current.condition`
- `current.humidity_pct`, `current.precip_probability_pct`
- `alert.title`, `alert.summary`, `alert.source`, `alert.issued_at`
- `hourly[]` entries with `time`, `temperature_c`, `precip_probability_pct`, `condition`

Keep text short, Korean-first, and safe for a living-room display.

## Live Data

- Use `--source open-meteo` for a working live data path today.
- Read `references/live-data.md` before claiming official alert support.
- Keep `Open-Meteo` for practical forecast integration and reserve official alerts for a separate KMA-grade source.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P0`
- Recommended surface: `fullscreen scene`
- Preferred data strategy: `mock-first, live-later`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard.
