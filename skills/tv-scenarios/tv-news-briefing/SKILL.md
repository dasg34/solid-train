---
name: tv-news-briefing
description: "Plan, assess, and implement a Korea-first TV news briefing surface for the Samsung Tizen GenUI PoC. Use when the user asks for headlines, lead stories, category groupings, summaries, or utterances such as `뉴스 보여줘`."
---

# TV News Briefing

Use this skill to scope, design, implement, or simulate a TV-friendly scenario surface in a separate Flutter app that consumes `genui` from pub.dev.

## Quick Start

Run these commands from this skill folder.

```bash
python3 scripts/generate_news_a2ui.py
python3 scripts/generate_news_a2ui.py --state loading
python3 scripts/generate_news_a2ui.py --state error
python3 scripts/generate_news_a2ui.py --source yonhap-rss --count 6
```

The script emits newline-delimited A2UI JSON messages for `createSurface`, `updateDataModel`, and `updateComponents`. Use `references/mock_surface.json` for deterministic offline demos, or `--source yonhap-rss` for the current live feed path.

## Workflow

1. Anchor the user intent around these requests: `뉴스 보여줘`, `헤드라인 보여줘`, `오늘 뉴스 요약해줘`.
2. Choose the surface shape early. Default to `fullscreen scene` or `side panel` and optimize for 10-foot readability.
3. Reuse `scripts/generate_news_a2ui.py` instead of hand-writing A2UI payloads for every turn.
4. Prioritize these payloads first: `lead story`, `headline hierarchy`, `category grouping`, `short summaries`.
5. Start with `mock-first` for offline demos, then use the bundled RSS live adapter when validating source freshness or attribution behavior.
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

- Use `--source yonhap-rss` for a working live feed path today.
- Read `references/live-data.md` before changing publishers or adding article-body summaries.
- Keep the live TV surface headline-first and source-attributed.

## Scenario Defaults

- Locale: `ko-KR`
- Time zone: `Asia/Seoul`
- Delivery phase: `P0`
- Recommended surface: `fullscreen scene` or `side panel`
- Preferred data strategy: `mock-first with live RSS path available`

## Reference

Read `references/feasibility.md` before widening the scope, adding live integrations, or combining this scenario into a larger dashboard.
