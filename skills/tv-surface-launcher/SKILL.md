---
name: tv-surface-launcher
description: >
  End-to-end TV surface orchestration for Korean natural-language display
  requests. Use this skill whenever the user wants to show something on the TV
  screen — '날씨 보여줘', '뉴스 알려줘', '주식 확인', '출근길 띄워줘', '오늘
  브리핑해줘', or any Korean request that implies fetching domain data and
  rendering it on a TV surface. This skill handles the presentation-first
  pipeline: scenario inference → tv_fetch → presentation JSON → save → local
  preview or app handoff.
---

# TV Surface Launcher

Turn a short Korean utterance into a prepared or previewed TV surface.

This skill is presentation-first. It uses:

- `tv-fetch` for normalized domain data
- `tv-a2ui-catalog` for the presentation JSON schema

## Current Reality

- Preferred output is semantic presentation JSON.
- Do NOT generate raw A2UI NDJSON in the default flow.
- Today the most reliable delivery path is local preview through
  `openclaw_tv_genui`.
- The Flutter app converts presentation payloads into deterministic A2UI
  internally.
- `tv_a2ui_launcher` is a historical name, but it now transports presentation
  JSON to the app.
- For runtime handoff, assume `tv_a2ui_launcher` is already installed on the
  target TV or Tizen environment and callable by name.
- If source lookup is needed, the local repo is
  `/Users/yohoho/work/tv_a2ui_launcher`, but do not block on finding it when
  the installed binary is the thing being used.

## Workflow

1. Infer the scenario from the noun phrase, not from verbs like 보여줘/알려줘.
2. If the scenario is unclear or a required fetch input is missing, ask one
   short Korean follow-up question. Ask only for the single missing piece.
3. Inspect the fetch contract: `tv_fetch describe <domain> --format pretty`
4. Fetch domain data: `tv_fetch <domain> [options] --format json`
5. Generate one presentation JSON object using `tv-a2ui-catalog`
6. Save it to `/tmp/tv-presentation/<domain>-<timestamp>.json`
7. Validate it with `tv_presentation_validate`
8. Deliver it using one of these modes:
   - Local preview: preferred default today
   - Runtime handoff: send the presentation JSON to the TV app
9. Report domain, source, assumptions, saved path, validation result, and
   preview or handoff result

## Raw JSON Only

The saved payload must be one raw JSON object only.

- Do NOT save NDJSON.
- Do NOT wrap the payload in Markdown fences.
- Do NOT prefix the file with commentary or headings.
- The first character of the file must be `{`.

Validate saved payloads before preview or handoff:

```bash
cd /Users/yohoho/work/tv_presentation_validate
meson setup builddir
meson compile -C builddir
./builddir/tv_presentation_validate /tmp/tv-presentation/<domain>-<timestamp>.json --format pretty
```

If `builddir` already exists, use:

```bash
cd /Users/yohoho/work/tv_presentation_validate
meson compile -C builddir
./builddir/tv_presentation_validate /tmp/tv-presentation/<domain>-<timestamp>.json --format pretty
```

## Domain Routing

Map Korean noun phrases to domains:

| Korean triggers | Domain |
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

If no domain is clear, ask one short clarification question.

## Fetch Rules

`tv_fetch` is a self-describing CLI. When unsure about domain options:

```bash
tv_fetch describe --format pretty
tv_fetch describe <domain> --format pretty
```

- Always fetch with `--format json` for pipeline use.
- Do not invent personal inputs such as location, route, watchlist, calendar,
  or flight data.
- Ask at most one short follow-up question per turn.
- Use `--source mock` for demo flows when appropriate.

## Presentation Rules

Generate the final surface payload with `tv-a2ui-catalog`.

- One strong hero metric is better than many competing numbers.
- Add `metrics`, `facts`, `chart`, and `alert` only when they materially help.
- Prefer stable `surfaceId` values such as `weather_today` or `finance_focus`.
- Keep Korean locale assumptions: `ko-KR`, `Asia/Seoul`.
- Do not emit footer fields or low-level component trees.

For examples, read:

- `skills/tv-a2ui-catalog/references/examples/weather_today.json`
- `skills/tv-a2ui-catalog/references/examples/finance_focus.json`

## Delivery Modes

### Local Preview

This is the default launch path today.

1. Save the generated JSON to `/tmp/tv-presentation/<domain>-<timestamp>.json`
2. Validate it with `tv_presentation_validate`
3. If the user wants to see it in the app, mirror the JSON into
   `openclaw_tv_genui/assets/presentation/<scenario>.json`
4. Reuse an existing scenario id when possible. Add a new scenario entry only
   when needed.
5. Preview with:

```bash
cd /Users/yohoho/work/openclaw_tv_genui
flutter run -d web-server --web-hostname=127.0.0.1 --web-port=3000 \
  --dart-define=OPENCLAW_DEFAULT_SCENARIO=<scenario>
```

Do not commit preview asset changes unless the user explicitly asks.

### Runtime Handoff

Use this when the user wants an external launch instead of local preview.

1. Save the generated JSON to `/tmp/tv-presentation/<domain>-<timestamp>.json`
2. Validate it with `tv_presentation_validate`
3. Launch with:

```bash
tv_a2ui_launcher --file /tmp/tv-presentation/<domain>-<timestamp>.json
```

4. Use `--dry-run --format pretty` when the user wants validation of the
   handoff without sending the launch request.

The launcher name is historical. It now sends presentation JSON using App
Control extra data key `json`.

Assume the launcher binary is already installed on the target TV or Tizen
environment. Only look in `/Users/yohoho/work/tv_a2ui_launcher` when the user
specifically asks about source, build, or packaging details.

## Response Contract

After finishing the workflow, briefly tell the user:

- Which domain and data source were used
- Any follow-up assumptions made
- Where the presentation JSON file was saved
- Whether `tv_presentation_validate` passed, and any warnings if present
- Whether local preview was started, and the preview URL if applicable
- If handoff was blocked, what integration was missing
