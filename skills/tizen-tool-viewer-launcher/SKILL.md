---
name: tizen-tool-viewer-launcher
description: >
  End-to-end TV surface orchestration for natural-language display
  requests. Use this skill whenever the user wants to show something on the TV
  screen — '날씨 보여줘', '뉴스 알려줘', '주식 확인', '출근길 띄워줘', '오늘
  브리핑해줘', 'show me the weather', 'show my commute', or any request that
  implies preparing domain data and
  rendering it on a TV surface. This skill handles the presentation-first
  pipeline: scenario inference → structured source data → presentation JSON →
  save → runtime handoff.
---

# Tizen Tool Viewer Launcher

Turn a short natural-language request into a prepared TV surface payload and
hand it off to the TV app.

This skill is presentation-first. It uses:

- `tizen-tool-presentation-catalog` for the presentation JSON schema

## Current Reality

- `tv_a2ui_launcher` is a historical name, but it now transports presentation
  JSON to the app.

## Workflow

1. Infer the scenario from the request intent and noun phrases, not from generic display verbs alone.
2. If the scenario is unclear or a required fetch input is missing, ask one
   short follow-up question. Ask only for the single missing piece.
3. Collect or accept structured domain data for the chosen scenario
4. Generate one presentation JSON object using
   `tizen-tool-presentation-catalog`
5. Save it to `/tmp/tv-presentation/<domain>-<timestamp>.json`
6. Deliver it with runtime handoff to the TV app
7. Report domain, source, assumptions, saved path, and
   handoff result

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

## Delivery

Use runtime handoff for delivery.

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
- If handoff was blocked, what integration was missing
