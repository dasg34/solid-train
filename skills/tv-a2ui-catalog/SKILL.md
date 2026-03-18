---
name: tv-a2ui-catalog
description: >
  A2UI component catalog and generation rules for Samsung Tizen TV.
  Teaches LLM agents how to generate valid A2UI v0.9 JSON that the TV app
  can render via genui's SurfaceController. If required rendering inputs are
  missing, ask the user a short follow-up question instead of inventing them.
---

# TV A2UI Catalog

This skill defines how to generate valid A2UI JSON for the OpenClaw TV app.
The TV app renders A2UI messages using genui's SurfaceController. You must
only use components listed here. Unknown components will display an error.

## Missing Information Rule

Do not invent required rendering inputs just to complete the JSON.

- If `theme.domain` cannot be determined from the request, ask the user.
- If the surface needs a user-specific value such as location, destination, keyword, or watchlist and the upstream data step did not provide it, ask the user.
- If the user asked for a vague shape like "보여줘" but the domain is clear, you may choose a reasonable `pattern`.
- Keep the follow-up short and targeted to the single missing piece.

## Raw Output Rule

When producing the final payload, emit raw NDJSON only.

- Do NOT wrap the payload in Markdown code fences such as ```json ... ``` or
  '''json ... '''.
- Do NOT add a `json` label, backticks, prose, bullets, or any explanation
  before or after the payload.
- The first character of the payload must be `{`.
- Every output line must be a JSON object.
- The fenced examples in this skill are documentation examples only. They are
  not the required final output format.

## A2UI Protocol

Generate three NDJSON messages (one JSON object per line) in this order:

### 1. createSurface

```json
{"version": "v0.9", "createSurface": {"surfaceId": "weather", "catalogId": "https://a2ui.org/specification/v0_9/standard_catalog.json", "theme": {"domain": "weather", "pattern": "immersive"}}}
```

- `surfaceId`: unique identifier for this surface. Treat it as an opaque ID, not as theme or layout metadata.
- `catalogId`: always `https://a2ui.org/specification/v0_9/standard_catalog.json`
- `theme.domain`: required. The scenario/domain such as `weather`, `news`, `finance`
- `theme.pattern`: required. One of `immersive`, `sidePanel`, `centerCard`, `topBanner`, `bottomRibbon`

### 2. updateDataModel

```json
{"version": "v0.9", "updateDataModel": {"surfaceId": "weather", "value": {"title": "서울 강남구", "currentTemp": "18°"}}}
```

- `surfaceId`: must match createSurface
- `value`: flat key-value map of all dynamic data
- May be omitted for purely static surfaces

### 3. updateComponents

```json
{"version": "v0.9", "updateComponents": {"surfaceId": "weather", "components": [...]}}
```

- `surfaceId`: must match createSurface
- `components`: array of component objects (see below)

## Components

The TV app supports these 11 components. Do NOT use any other component type.

The base layout and text items come from genui's standard catalog. The TV app
also registers these custom TV items:
- `Inset`
- `Wrap`
- `LineChart`
- `BarChart`

Every component must have:
- `id` — unique string identifier
- `component` — type name from the table below
- `weight` — (optional) integer for flex sizing inside Row/Column

One component MUST have `id: "root"` — the render entry point.

### Text

Display text, either static or data-bound.

| Property | Required | Values |
|----------|----------|--------|
| `text` | yes | `"literal string"` or `{"path": "/dataKey"}` |
| `variant` | no | `h1`, `h2`, `h3`, `h4`, `h5`, `body`, `caption` |

### Column

Vertical layout container.

| Property | Required | Values |
|----------|----------|--------|
| `children` | yes | array of component IDs, or template object |
| `justify` | no | `start`, `center`, `end`, `spaceBetween`, `spaceAround` |
| `align` | no | `start`, `center`, `end`, `stretch` |

### Row

Horizontal layout container. Same properties as Column.

### Inset

Padding container for a single child.

| Property | Required | Values |
|----------|----------|--------|
| `child` | yes | single component ID |
| `all` | no | number, uniform padding on every side |
| `horizontal` | no | number, horizontal padding in logical pixels |
| `vertical` | no | number, vertical padding in logical pixels |

Use `all` for one-value padding, or `horizontal` and `vertical` together for
TV-safe spacing around a child card or text block.

### Wrap

Flow layout for multiple children that should wrap onto new rows.

| Property | Required | Values |
|----------|----------|--------|
| `children` | yes | array of component IDs |
| `spacing` | no | number, horizontal gap between children |
| `runSpacing` | no | number, vertical gap between rows |
| `alignment` | no | `start`, `center`, `end`, `spaceBetween`, `spaceAround`, `spaceEvenly` |
| `runAlignment` | no | `start`, `center`, `end`, `spaceBetween`, `spaceAround`, `spaceEvenly` |
| `crossAlign` | no | `start`, `center`, `end` |

Use this for chip-like summaries or small card groups that should reflow
without forcing a strict `Row`.

### Card

Visual container with elevation and rounded corners.

| Property | Required | Values |
|----------|----------|--------|
| `child` | yes | single component ID |

### Icon

Material Design icon.

| Property | Required | Values |
|----------|----------|--------|
| `name` | yes | one of the valid icon names below |

Valid icon names:
```
accountCircle, add, arrowBack, arrowForward, attachFile, calendarToday,
call, camera, check, close, delete, download, edit, error, event,
favorite, favoriteOff, folder, help, home, info, locationOn, lock,
lockOpen, mail, menu, moreHoriz, moreVert, notifications,
notificationsOff, payment, person, phone, photo, print, refresh,
search, send, settings, share, shoppingCart, star, starHalf, starOff,
upload, visibility, visibilityOff, warning
```

Using any other name will render a broken image.

### Divider

Visual separator line.

| Property | Required | Values |
|----------|----------|--------|
| `axis` | no | `horizontal` (default), `vertical` |

### Button

Clickable action trigger.

| Property | Required | Values |
|----------|----------|--------|
| `child` | yes | component ID (usually a Text) |
| `action` | yes | `{"event": {"name": "eventName"}}` |
| `variant` | no | `primary`, `borderless` |

### LineChart

Compact trend chart for ordered numeric values.

| Property | Required | Values |
|----------|----------|--------|
| `values` | yes | number array or `{"path": "/dataKey"}` |
| `labels` | no | string array or `{"path": "/dataKey"}` |
| `height` | no | number, usually `120`-`160` |
| `strokeColor` | no | hex color string like `"#7DE8D5"` |
| `fillStartColor` | no | hex color string |
| `fillEndColor` | no | hex color string, `"#00000000"` allowed |
| `showGrid` | no | boolean |
| `showLabels` | no | boolean |

Use for short time-series such as hourly weather, 5-day exchange trend, or
weekly activity movement. Prefer 4-8 points for TV readability.

### BarChart

Compact comparison chart for category values.

| Property | Required | Values |
|----------|----------|--------|
| `values` | yes | number array or `{"path": "/dataKey"}` |
| `labels` | no | string array or `{"path": "/dataKey"}` |
| `height` | no | number, usually `120`-`160` |
| `positiveColor` | no | hex color string |
| `negativeColor` | no | hex color string |
| `baselineColor` | no | hex color string |
| `showGrid` | no | boolean |
| `showLabels` | no | boolean |

Use for comparisons such as stock moves, category scores, spending buckets, or
temperature/rain probability by slot. Keep label count low, usually 3-6.

## Data Binding

Dynamic values reference the dataModel using path syntax:
```json
{"text": {"path": "/currentTemp"}}
```

Static values are literal strings:
```json
{"text": "서울 강남구"}
```

Every path must have a corresponding key in the `updateDataModel` value map.

Number arrays for charts should be emitted as actual JSON numbers, not strings,
unless the values are already formatted for display elsewhere.

You do not need to render every available data field. Select the subset that
best supports the surface. It is acceptable to omit low-value or secondary
data when it would reduce clarity.

## Dynamic-Length Lists

Column and Row support template children for variable-length data:

```json
{
  "id": "itemList",
  "component": "Column",
  "children": {"componentId": "itemRow", "path": "/items"}
}
```

This renders one `itemRow` component per entry in the `/items` array.

`Wrap` currently expects an explicit `children` array of component IDs.

## TV UX Principles

- **10-foot experience** — understandable from across the room in seconds
- **Large typography** — use `h1`-`h3` for primary info, `body`/`caption` for details
- **Strong visual hierarchy** — generous spacing, clear grouping with Cards
- **Glanceable** — keep under ~40 components per surface
- **Chart restraint** — one chart per card is usually enough on TV
- **No walls of text** — short labels, concise values
- **Selective data use** — show only the data that strengthens the current
  surface; not every fetched field must be displayed
- **Korean locale** — ko-KR, Asia/Seoul timezone
- **Domain-specific** — prefer purpose-built surfaces over generic layouts

## Surface Patterns

Choose `theme.pattern` based on how much screen space the content needs and
whether the user is actively watching TV content alongside it.

| Pattern | When to use | theme.pattern |
|---------|-------------|---------------|
| Immersive (풀 캔버스) | rich multi-section content that deserves full attention — weather detail, sports scores, travel plans | `immersive` |
| Side Panel (사이드 패널) | supplementary info while TV content stays visible — news headlines, smart home status | `sidePanel` |
| Center Card (센터 카드) | one key metric or brief summary — quick stock check, delivery ETA, calendar next-up | `centerCard` |
| Top Banner (상단 배너) | urgent one-line alert that should not block viewing — severe weather warning, breaking news flash | `topBanner` |
| Bottom Ribbon (하단 리본) | ambient persistent status — commute ETA, air quality index, live score ticker | `bottomRibbon` |

The TV app applies a themed background based on `theme.domain`:
- `weather` → warm atmospheric gradient
- `news` → muted news backdrop
- `schedule` → cool schedule backdrop
- everything else → standard dark theme (#12212D)

## Common Mistakes

- Do NOT nest components inside `createSurface` — use `updateComponents`
- Do NOT reference a data path not defined in `updateDataModel`
- Do NOT use component IDs not defined in the components array
- Do NOT forget the `"root"` component — nothing renders without it
- Do NOT use icon names outside the valid list
- Do NOT send chart arrays of mismatched lengths unless labels are optional
- Do NOT cram dense dashboards with many tiny charts onto one TV surface
- Do NOT try to display every available field if it weakens clarity
- Do NOT create more than ~40 components per surface
- Do NOT force one fixed `theme.pattern` for a domain

## Minimal Example

A simple card with a title and a value:

```
{"version": "v0.9", "createSurface": {"surfaceId": "weather", "catalogId": "https://a2ui.org/specification/v0_9/standard_catalog.json", "theme": {"domain": "weather", "pattern": "centerCard"}}}
{"version": "v0.9", "updateDataModel": {"surfaceId": "weather", "value": {"title": "서울 날씨", "temp": "18°", "condition": "맑음"}}}
{"version": "v0.9", "updateComponents": {"surfaceId": "weather", "components": [{"id": "root", "component": "Column", "justify": "center", "children": ["heroCard"]}, {"id": "heroCard", "component": "Card", "child": "cardContent"}, {"id": "cardContent", "component": "Column", "children": ["titleText", "tempRow"]}, {"id": "titleText", "component": "Text", "text": {"path": "/title"}, "variant": "h3"}, {"id": "tempRow", "component": "Row", "justify": "spaceBetween", "children": ["tempText", "conditionText"]}, {"id": "tempText", "component": "Text", "text": {"path": "/temp"}, "variant": "h1"}, {"id": "conditionText", "component": "Text", "text": {"path": "/condition"}, "variant": "body"}]}}
```

## Validation

After generating A2UI JSON, run `tv_a2ui_validate` to catch errors before
the TV app receives it:

```bash
tv_a2ui_validate output.json
```

It checks NDJSON structure, message ordering, surfaceId consistency, theme,
component types, icon names, referential integrity, data bindings, and
component count. Error messages include the exact component ID, property,
and valid values so you can fix issues directly.

To read from stdin (useful when piping generated output):

```bash
echo "$NDJSON" | tv_a2ui_validate --stdin
```

## Full Examples

See `references/examples/` for production-validated A2UI JSON:
- `weather.json` — immersive weather briefing with hourly forecast
- `news.json` — side panel news with headline list
- `card_briefing.json` — center card financial snapshot
