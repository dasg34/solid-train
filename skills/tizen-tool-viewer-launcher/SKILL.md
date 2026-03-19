---
name: tizen-tool-viewer-launcher
description: >
  End-to-end TV surface orchestration for natural-language display
  requests. Use this skill whenever the user wants to show something on the TV
  screen — '날씨 보여줘', '뉴스 알려줘', '주식 확인', '출근길 띄워줘', '오늘
  브리핑해줘', 'show me the weather', 'show my commute', or any request that
  implies preparing domain data and
  rendering it on a TV surface. This skill handles the presentation-first
  pipeline: scenario inference → structured source data → presentation JSON → save → local
  preview or app handoff.
---

# Tizen Tool Viewer Launcher

Turn a short natural-language request into a prepared or previewed TV surface.

This skill is presentation-first. It uses:

- `tizen-tool-presentation-catalog` for the presentation JSON schema

## Current Reality

- Preferred output is semantic presentation JSON.
- Do NOT generate raw A2UI NDJSON in the default flow.
- Today the most reliable delivery path is local preview through
  `openclaw_tv_genui`.
- The Flutter app converts presentation payloads into deterministic A2UI
  internally.
- `tv_a2ui_launcher` is a historical name, but it now transports presentation
  JSON to the app.

## Workflow

1. Infer the scenario from the request intent and noun phrases, not from generic display verbs alone.
2. If the scenario is unclear or a required fetch input is missing, ask one
   short follow-up question. Ask only for the single missing piece.
3. Collect or accept structured domain data for the chosen scenario
4. Generate one presentation JSON object using `tizen-tool-presentation-catalog`
5. Save it to `/tmp/tv-presentation/<domain>-<timestamp>.json`
6. Deliver it using one of these modes:
   - Local preview: preferred default today
   - Runtime handoff: send the presentation JSON to the TV app
7. Report domain, source, assumptions, saved path, and
   preview or handoff result

## Raw JSON Only

The saved payload must be one raw JSON object only.

- Do NOT save NDJSON.
- Do NOT wrap the payload in Markdown fences.
- Do NOT prefix the file with commentary or headings.
- The first character of the file must be `{`.

## Domain Routing

Map request phrases to domains:

| Example triggers | Domain |
|---|---|
| 날씨, 기온, 비, 강수, 체감온도 | `weather` |
| 뉴스, 헤드라인, 속보 | `news` |
| 일정, 캘린더, 회의, 행사 | `schedule` |
| 출근, 퇴근, 언제 나가, 길찾기, 통근 | `commute` |
| 주식, 환율, 시황, 증시, 금융 | `finance` |
| 경기, 점수, 순위, 스포츠 | `sports` |
| 공항, 비행기, 탑승, 게이트, 출국 | `travel` |
| 재난, 경보, 지진, 특보 | `emergency` |
| 브리핑, 모닝 브리핑, 오늘 요약, 데일리 | `daily` |
| 가족 게시판 | `family` |
| 배달, 주문 상태, 도착 예정 | `meal-delivery` |
| 미디어, 에피소드, 출연진 | `media` |
| 쇼핑, 비교, 추천 비교 | `shopping` |
| 스마트홈, 집 상태, 문 잠김, 실내 온도 | `smart-home` |
| 웰니스, 운동, 수면, 스트레칭 | `wellness` |

The trigger examples are illustrative, not language-limited.

If no domain is clear, ask one short clarification question.

## Source Data Rules

- Structured source data may come from any suitable upstream system.
- Do not invent personal inputs such as location, route, watchlist, calendar,
  or flight data.
- Ask at most one short follow-up question per turn when a required input is missing.
- Preserve source semantics when mapping upstream data into presentation JSON.

## Presentation Rules

Generate the final surface payload with `tizen-tool-presentation-catalog`.

- One strong hero metric is better than many competing numbers.
- Add `metrics`, `facts`, `chart`, and `alert` only when they materially help.
- Prefer stable `surfaceId` values such as `weather_today` or `finance_focus`.
- Do not emit footer fields or low-level component trees.

For examples, read:

- `skills/tizen-tool-presentation-catalog/references/examples/weather_today.json`
- `skills/tizen-tool-presentation-catalog/references/examples/finance_focus.json`

## Delivery Modes

### Local Preview

This is the default launch path today.

1. Save the generated JSON to `/tmp/tv-presentation/<domain>-<timestamp>.json`
2. If the user wants to see it in the app, mirror the JSON into
   `openclaw_tv_genui/assets/presentation/<scenario>.json`
3. Reuse an existing scenario id when possible. Add a new scenario entry only
   when needed.
4. Preview with:

```bash
cd /Users/yohoho/work/openclaw_tv_genui
flutter run -d web-server --web-hostname=127.0.0.1 --web-port=3000 \
  --dart-define=OPENCLAW_DEFAULT_SCENARIO=<scenario>
```

Do not commit preview asset changes unless the user explicitly asks.

### Runtime Handoff

Use this when the user wants an external launch instead of local preview.

1. Save the generated JSON to `/tmp/tv-presentation/<domain>-<timestamp>.json`
2. Launch with:

```bash
tv_a2ui_launcher --file /tmp/tv-presentation/<domain>-<timestamp>.json
```

3. Use `--dry-run --format pretty` when the user wants to inspect the
   handoff without sending the launch request.

The launcher name is historical. It now sends presentation JSON using App
Control extra data key `json`.

## Response Contract

After finishing the workflow, briefly tell the user:

- Which domain and data source were used
- Any follow-up assumptions made
- Where the presentation JSON file was saved
- Whether local preview was started, and the preview URL if applicable
- If handoff was blocked, what integration was missing
