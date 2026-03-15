---
name: tv-emergency-alert-mode
description: "Plan, assess, and implement a Korea-first TV emergency or alert surface for the Samsung Tizen GenUI PoC. Use when the user or system needs severe weather, device warnings, security events, or evacuation guidance with high-priority treatment."
---

# TV Emergency Alert Mode

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_emergency_alert_a2ui.py
python3 scripts/generate_emergency_alert_a2ui.py --source kma-combined
python3 scripts/generate_emergency_alert_a2ui.py --source kma-earthquake --min-magnitude 3.0
python3 scripts/generate_emergency_alert_a2ui.py --state loading
python3 scripts/generate_emergency_alert_a2ui.py --state error
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. The default CLI path keeps `--source mock` for deterministic demos, and `--source kma-combined` prefers official KMA special reports before falling back to recent KMA earthquake events.

## Workflow

1. Anchor the user intent around these requests: `긴급 알림 보여줘`, `재난 정보 보여줘`, `경보 모드 켜줘`.
2. Choose the surface shape early. Default to `fullscreen alert` or `bottom ribbon` and optimize for 10-foot readability.
3. Reuse `scripts/generate_emergency_alert_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `alert type`, `affected area`, `recommended action`, `expiry`, `confidence`.
5. Use `official-feed-first`: KMA special reports first, recent KMA earthquake events second, and empty state when neither source has a TV-worthy item.
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

- Live paths:
  - `--source kma-combined`
  - `--source kma-special-report`
  - `--source kma-earthquake`
- Default live behavior prefers KMA `특보/예비특보/속보/기상정보` pages and falls back to recent earthquake entries above the configured magnitude threshold.
- Recommended live commands:

```bash
python3 scripts/generate_emergency_alert_a2ui.py --source kma-combined
python3 scripts/generate_emergency_alert_a2ui.py --source kma-earthquake --min-magnitude 3.0 --max-age-days 7
```

- If neither source has an item worth showing, the skill emits the empty state instead of inventing an alert.
- Read `references/live-data.md` before changing thresholds or extending this skill into non-KMA sources.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P1`
- Recommended surface: `fullscreen alert` or `bottom ribbon`
- Preferred data strategy: `official-feed-first`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard. Read `references/live-data.md` when you need the current KMA source order, threshold rules, and example commands.
