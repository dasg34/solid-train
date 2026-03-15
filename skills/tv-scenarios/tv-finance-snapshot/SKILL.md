---
name: tv-finance-snapshot
description: "Plan, assess, and implement a Korea-first TV finance snapshot surface for the Samsung Tizen GenUI PoC. Use when the user asks for watchlist movement, exchange rates, budget reminders, or market highlights."
---

# TV Finance Snapshot

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_finance_a2ui.py
python3 scripts/generate_finance_a2ui.py --source naver-public
python3 scripts/generate_finance_a2ui.py --source naver-public --watchlist '005930:삼성전자,000660:SK하이닉스,035420:NAVER'
python3 scripts/generate_finance_a2ui.py --state loading
python3 scripts/generate_finance_a2ui.py --state error
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. `references/mock_surface.json` remains the offline baseline, and `--source naver-public` now combines domestic market quotes from Npay Securities polling with KRW exchange-rate snapshots derived from Frankfurter.

## Workflow

1. Anchor the user intent around these requests: `주식 보여줘`, `환율 알려줘`, `시장 요약해줘`.
2. Choose the surface shape early. Default to `restrained dashboard` and optimize for 10-foot readability.
3. Reuse `scripts/generate_finance_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `watchlist movement`, `exchange rates`, `budget reminders`, `market events`.
5. Start with the built-in `naver-public` live path for Korea-first demos, but keep copy and alerting conservative so the card never reads like trading advice.
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

- Built-in live path:
  - `domestic indices and watchlist`: Npay Securities polling endpoint
  - `USD/KRW`, `100JPY/KRW`: Frankfurter API with ECB-based rate snapshots
- Keep the watchlist domestic and short. The current adapter expects `6-digit` Korean stock codes such as `005930`.
- Read `references/live-data.md` before changing source defaults or widening the financial scope.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P2`
- Recommended surface: `restrained dashboard`
- Preferred data strategy: `Korea-first live snapshot with conservative copy, mock fallback offline`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard. Read `references/live-data.md` when you need current commands, source notes, or trust-boundary reminders.
