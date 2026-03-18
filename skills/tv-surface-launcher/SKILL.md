---
name: tv-surface-launcher
description: >
  End-to-end TV surface orchestration for Korean natural-language display
  requests. Use this skill whenever the user wants to show something on the TV
  screen — '날씨 보여줘', '뉴스 알려줘', '주식 확인', '출근길 띄워줘', '오늘
  브리핑해줘', or any Korean request that implies fetching domain data and
  rendering it on a TV surface. This skill handles the complete pipeline:
  data fetch → A2UI JSON generation → file save → validation → launch.
  Do not wait for the user to mention 'A2UI', 'tv_fetch', or 'launcher' —
  trigger on the intent to display information on TV.
---

# TV Surface Launcher

Turn a short Korean utterance into a launched TV surface. This skill is
self-contained — it covers the full pipeline from data fetch through A2UI
generation to launch without requiring other skills to be loaded.

## Workflow

1. Infer the scenario from the noun phrase (not from verbs like 보여줘/알려줘).
2. If the scenario is unclear or a required fetch input is missing, ask one
   short Korean follow-up question and wait. Ask only for the single missing
   piece — do not invent personal inputs.
3. Inspect the fetch contract: `tv_fetch describe <domain> --format pretty`
4. Fetch domain data: `tv_fetch <domain> [options] --format json`
5. Generate A2UI v0.9 NDJSON from the fetched data.
6. Save NDJSON to `/tmp/tv-a2ui/<domain>-<timestamp>.ndjson`.
7. Validate: `tv_a2ui_validate <file>`
8. Launch: `cat <file> | tv_a2ui_launcher`
9. Report domain, source, file path, validation, and launch result.

## Raw NDJSON Only

The payload that gets saved, validated, and launched must be raw NDJSON only.

- Do NOT save or pipe Markdown code fences such as ```json ... ``` or
  '''json ... '''.
- Do NOT prefix the payload with `json`, commentary, bullets, or headings.
- The file content must start with `{` on the first line.
- If fenced output appears by accident, treat it as invalid output and
  regenerate the payload in raw NDJSON form before saving it.

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

If no domain is clear, ask a short clarification question.

## Fetch Rules

tv_fetch is a self-describing CLI. When unsure about domain options:

```bash
tv_fetch describe --format pretty           # full CLI structure
tv_fetch describe <domain> --format pretty  # domain-specific options
```

- Always fetch with `--format json` for pipeline use.
- Do not invent personal inputs (location, route, watchlist, calendar, flight).
  Ask the user if a required input is missing.
- Ask at most one short follow-up question per turn.
- Use `--source mock` for demo/test purposes when appropriate.

## A2UI v0.9 Protocol

Generate exactly three NDJSON lines (one JSON object per line):

### Line 1 — createSurface

```json
{"version": "v0.9", "createSurface": {"surfaceId": "<id>", "catalogId": "https://a2ui.org/specification/v0_9/standard_catalog.json", "theme": {"domain": "<domain>", "pattern": "<pattern>"}}}
```

- `surfaceId`: unique ID, e.g. `weather-20260318-161500`
- `catalogId`: always `https://a2ui.org/specification/v0_9/standard_catalog.json`
- `theme.domain`: the scenario domain
- `theme.pattern`: one of `immersive`, `sidePanel`, `centerCard`, `topBanner`, `bottomRibbon`
- `theme.scale`: optional. Use `compact` for sparse, narrow, or single-card
  layouts that should not leave a large empty panel. Use `expanded` only when
  the content is unusually dense.

### Line 2 — updateDataModel

```json
{"version": "v0.9", "updateDataModel": {"surfaceId": "<id>", "value": {"key": "value"}}}
```

- `surfaceId`: must match createSurface
- `value`: flat key-value map of all dynamic data
- May be omitted for purely static surfaces

### Line 3 — updateComponents

```json
{"version": "v0.9", "updateComponents": {"surfaceId": "<id>", "components": [...]}}
```

- `surfaceId`: must match createSurface
- `components`: array of component objects
- One component MUST have `id: "root"` — the render entry point

## Surface Patterns

| Pattern | When to use | `theme.pattern` |
|---|---|---|
| Immersive | Rich multi-section: weather detail, sports, travel | `immersive` |
| Side Panel | Supplementary while TV plays: news, smart home | `sidePanel` |
| Center Card | One key metric: stock check, delivery ETA | `centerCard` |
| Top Banner | Urgent one-line alert: severe weather, breaking news | `topBanner` |
| Bottom Ribbon | Ambient persistent: commute ETA, live score | `bottomRibbon` |

If the chosen pattern still feels visually too large for the content, set
`theme.scale` to `compact`.

Background themes: `weather` → warm gradient, `news` → muted backdrop,
`schedule` → cool backdrop, others → dark (#12212D).

## Card-First Layout Rule

When composing the final A2UI, prefer card-based sections with visible inner
padding.

- Major sections should usually be `Card` containers, not bare root-level text
  blocks.
- The safest default structure is `Card -> Inset -> Column` or
  `Card -> Inset -> Row`.
- Use `Inset` inside cards so text, icons, and charts do not touch the card
  edge.
- A good default is `Inset all: 24`, then adjust upward for denser content.
- In side panels and center-card layouts, be especially aggressive about using
  cards for section separation.
- Charts should usually live inside the same card as their title and summary
  values, not as a naked graph block.

## Components

11 allowed component types. Using any other type renders an error.

Every component needs: `id` (unique string), `component` (type name).
Optional on all: `weight` (integer, flex sizing in Row/Column).

### Layout Components

**Column** — Vertical container.
`children`: ID array or template `{"componentId": "id", "path": "/array"}`.
Optional: `justify` (start|center|end|spaceBetween|spaceAround), `align` (start|center|end|stretch).

**Row** — Horizontal container. Same props as Column.

**Inset** — Padding wrapper for a single child.
`child`: single component ID. Optional: `all`, `horizontal`, `vertical` (pixel values).
Prefer `Inset` inside `Card` for inner padding.

**Wrap** — Flow layout that wraps onto new rows.
`children`: ID array. Optional: `spacing`, `runSpacing`, `alignment`, `runAlignment`, `crossAlign`.

**Card** — Elevated container with rounded corners.
`child`: single component ID.
Prefer `Card` for hero summaries, section groups, and chart blocks.

### Content Components

**Text** — Display text.
`text`: `"literal"` or `{"path": "/dataKey"}`.
Optional: `variant` (h1|h2|h3|h4|h5|body|caption).

**Icon** — Material Design icon.
`name`: must be from the valid icon list below.

**Divider** — Separator line.
Optional: `axis` (horizontal|vertical, default horizontal).

**Button** — Clickable action.
`child`: component ID (usually Text). `action`: `{"event": {"name": "eventName"}}`.
Optional: `variant` (primary|borderless).

### Chart Components

**LineChart** — Trend line for ordered numeric values.
`values`: number array or `{"path": "/key"}`.
Optional: `labels`, `height` (120-160), `strokeColor`, `fillStartColor`, `fillEndColor`, `showGrid`, `showLabels`.
Do not emit a line chart without nearby text explaining the metric and key values.

**BarChart** — Comparison bars for category values.
`values`: number array or `{"path": "/key"}`.
Optional: `labels`, `height` (120-160), `positiveColor`, `negativeColor`, `baselineColor`, `showGrid`, `showLabels`.
Do not emit a bar chart without a title and nearby max/min or change summary text.

### Valid Icon Names

```
accountCircle, add, arrowBack, arrowForward, attachFile, calendarToday,
call, camera, check, close, delete, download, edit, error, event,
favorite, favoriteOff, folder, help, home, info, locationOn, lock,
lockOpen, mail, menu, moreHoriz, moreVert, notifications,
notificationsOff, payment, person, phone, photo, print, refresh,
search, send, settings, share, shoppingCart, star, starHalf, starOff,
upload, visibility, visibilityOff, warning
```

Using any other name renders a broken image.

## Data Binding

- Dynamic values: `{"text": {"path": "/currentTemp"}}` — every path must
  exist in the updateDataModel value map.
- Static values: `{"text": "서울 강남구"}`
- Chart arrays: use JSON numbers, not strings.
- Dynamic-length lists: `{"children": {"componentId": "itemRow", "path": "/items"}}`

Select only the data fields that strengthen the surface — not every fetched
field needs to be displayed.

## TV UX Principles

- **10-foot experience** — readable from across the room in seconds
- **Large typography** — h1-h3 for primary info, body/caption for details
- **Glanceable** — keep under ~40 components per surface
- **Chart restraint** — one chart per card is usually enough
- **No walls of text** — short labels, concise values
- **Korean locale** — ko-KR, Asia/Seoul timezone

## Common Mistakes

- Do NOT nest components inside createSurface — use updateComponents
- Do NOT reference a data path not defined in updateDataModel
- Do NOT use component IDs not listed in the components array
- Do NOT forget the `"root"` component — nothing renders without it
- Do NOT use icon names outside the valid list
- Do NOT create more than ~40 components per surface
- Do NOT force one fixed pattern for a domain

## Save, Validate, Launch

```bash
mkdir -p /tmp/tv-a2ui
cat > /tmp/tv-a2ui/<domain>-<timestamp>.ndjson <<'EOF'
<line 1: createSurface>
<line 2: updateDataModel>
<line 3: updateComponents>
EOF

tv_a2ui_validate /tmp/tv-a2ui/<domain>-<timestamp>.ndjson
cat /tmp/tv-a2ui/<domain>-<timestamp>.ndjson | tv_a2ui_launcher
```

For dry-run: `cat <file> | tv_a2ui_launcher --dry-run --format pretty`

Do not save generated NDJSON into the git repo unless the user explicitly asks.

## Response Contract

After finishing the workflow, briefly tell the user:

- Which domain and source were used
- Any follow-up assumptions made
- Where the NDJSON file was saved
- Whether validation passed
- Whether the launcher executed successfully

## Full Examples

For production-validated A2UI when building complex surfaces, read these
reference files:

- `skills/tv-a2ui-catalog/references/examples/weather.json` — immersive weather with hourly forecast and chart
- `skills/tv-a2ui-catalog/references/examples/news.json` — side panel news with headline sections
- `skills/tv-a2ui-catalog/references/examples/card_briefing.json` — center card finance snapshot with bar chart
