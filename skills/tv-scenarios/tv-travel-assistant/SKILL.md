---
name: tv-travel-assistant
description: "Plan, assess, and implement a Korea-first TV travel assistant surface for the Samsung Tizen GenUI PoC. Use when the user asks for boarding countdowns, gate changes, hotel details, reservation codes, or trip checkpoints."
---

# TV Travel Assistant

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_travel_a2ui.py
python3 scripts/generate_travel_a2ui.py --source airport-kr
python3 scripts/generate_travel_a2ui.py --source airport-kr --flight-number KE913
python3 scripts/generate_travel_a2ui.py --state loading
python3 scripts/generate_travel_a2ui.py --state error
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. `references/mock_surface.json` stays available for template work, and `--source airport-kr` turns the skill into a live Incheon Airport departure helper.

## Workflow

1. Anchor the user intent around these requests: `여행 일정 보여줘`, `비행기 정보 보여줘`, `출국 준비 뭐 남았어?`.
2. Choose the surface shape early. Default to `fullscreen scene` or `split composition` and optimize for 10-foot readability.
3. Reuse `scripts/generate_travel_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `boarding countdown`, `gate`, `check-in counter`, `airport congestion`, `destination local time`.
5. Scope the first live version as `airport departure helper`, not a full reservation hub. Reuse official departure and congestion sources before widening to hotels or codes.
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

- `--source airport-kr` fetches official Incheon Airport departure data.
- The live card is optimized for `출국 준비 뭐 남았어?`, `비행기 정보 보여줘`, `공항 출발 정보 보여줘`.
- Useful filters:
  - `--flight-number KE913`
  - `--terminal T2`
  - `--destination-code KIX`
  - `--date 20260315 --from-time 1200 --to-time 1659`
- Read `references/live-data.md` before changing the live source mix or trust policy.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P1`
- Recommended surface: `fullscreen scene` or `split composition`
- Preferred data strategy: `airport-helper-live, itinerary-sync-later`

## Reference

Read `references/feasibility.md` before widening the scope, and `references/live-data.md` before changing the live airport adapter.
