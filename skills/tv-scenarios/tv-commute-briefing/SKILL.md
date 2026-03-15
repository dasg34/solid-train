---
name: tv-commute-briefing
description: "Plan, assess, and implement a Korea-first TV commute briefing surface for the Samsung Tizen GenUI PoC. Use when the user asks for travel time, departure recommendations, traffic risk, route options, or utterances such as `출근길 보여줘`."
---

# TV Commute Briefing

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_commute_a2ui.py
python3 scripts/generate_commute_a2ui.py --source osrm --origin "서울시청" --destination "강남역"
python3 scripts/generate_commute_a2ui.py --state loading
python3 scripts/generate_commute_a2ui.py --state error
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. The default CLI path keeps `--source mock` for deterministic offline demos, and `--source osrm` uses live geocoding and route estimation through Nominatim plus OSRM.

## Workflow

1. Anchor the user intent around these requests: `출근길 보여줘`, `언제 출발해야 해?`, `교통 상황 알려줘`.
2. Choose the surface shape early. Default to `side panel` or `center card` and optimize for 10-foot readability.
3. Reuse `scripts/generate_commute_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `next destination`, `travel time`, `recommended departure`, `traffic risk`, `alternate routes`.
5. Use `mock-first` for offline demos, but prefer `--source osrm` once origin, destination, and privacy masking rules are clear.
6. Include loading, empty, degraded, and error states before polishing animation or secondary controls.
7. Treat A2UI payloads, agent responses, and backend data as untrusted input.
8. Reuse local references:
   - `genui/examples/simple_chat` for minimal `SurfaceController` wiring
   - `genui/examples/travel_app` for domain-specific catalogs and tool-backed patterns
   - `genui/examples/verdure/client` for app shell, routing, and polished loading states
   - `genui/packages/genui_a2a` only when direct A2A transport is required

## Input Contract

Use `references/mock_surface.json` as the baseline schema.

- `title`
- `headline`
- `primaryMetrics[]` with `label`, `value`, optional `detail`
- `sections[]` with `title` and `items[]`
- `items[]` with `label`, `value`, optional `detail`, optional `icon`
- `alert` with `title`, `summary`, optional `meta`, optional `icon`
- `actions[]` with `label`, `event`
- `footer`

Keep text short, Korean-first, and safe for a living-room display.

## Live Data

- Live path:
  - `--source osrm`
  - Geocoding: OpenStreetMap Nominatim
  - Routing: OSRM public demo server
- Recommended live command:

```bash
python3 scripts/generate_commute_a2ui.py --source osrm --origin "서울시청" --destination "강남역" --arrive-by 2026-03-16T09:00:00+09:00
```

- Current live adapter estimates route time, distance, departure recommendation, and alternate-route gap.
- It does not include real-time traffic congestion or public-transit delays yet, so the output should be presented as route guidance rather than guaranteed ETA.
- Read `references/live-data.md` before changing source defaults or exposing more exact location text on TV.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P1`
- Recommended surface: `side panel` or `center card`
- Preferred data strategy: `mock-first, OSRM route estimate when privacy allows`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard. Read `references/live-data.md` for the current live command, source caveats, and masking guidance.
