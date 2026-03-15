---
name: tv-meal-delivery-status
description: "Plan, assess, and implement a Korea-first TV meal or delivery status surface for the Samsung Tizen GenUI PoC. Use when the user asks for order progress, ETA, courier status, or next actions for a recent order."
---

# TV Meal Delivery Status

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_meal_delivery_a2ui.py
python3 scripts/generate_meal_delivery_a2ui.py --state loading
python3 scripts/generate_meal_delivery_a2ui.py --state error
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. Use `references/mock_surface.json` as the default payload and replace it later with a live adapter only after the data source, auth model, and trust policy are clear.

## Workflow

1. Anchor the user intent around these requests: `배달 어디쯤이야?`, `주문 상태 보여줘`, `음식 언제 와?`.
2. Choose the surface shape early. Default to `center card` or `bottom ribbon` and optimize for 10-foot readability.
3. Reuse `scripts/generate_meal_delivery_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `order progress`, `ETA`, `courier status`, `reorder or contact action`.
5. Start with `mock-first, partner-api-later` and expand to live data only after the dependency list in `references/feasibility.md` is stable.
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

- This skill currently ships with `mock-only` execution.
- Add live adapters only after the source, account model, and trust boundary are fixed for this scenario.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P2`
- Recommended surface: `center card` or `bottom ribbon`
- Preferred data strategy: `mock-first, partner-api-later`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard.
