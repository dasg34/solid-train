---
name: tv-a2ui-catalog
description: >
  A2UI component catalog and generation rules for Samsung Tizen TV.
  Teaches LLM agents how to generate valid A2UI v0.9 JSON that the TV app
  can render via genui's SurfaceController.
---

# TV A2UI Catalog

This skill defines how to generate valid A2UI JSON for the OpenClaw TV app.
The TV app renders A2UI messages using genui's SurfaceController. You must
only use components listed here. Unknown components will display an error.

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

The TV app supports these 9 components. Do NOT use any other component type.

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

## TV UX Principles

- **10-foot experience** — understandable from across the room in seconds
- **Large typography** — use `h1`-`h3` for primary info, `body`/`caption` for details
- **Strong visual hierarchy** — generous spacing, clear grouping with Cards
- **Glanceable** — keep under ~40 components per surface
- **Chart restraint** — one chart per card is usually enough on TV
- **No walls of text** — short labels, concise values
- **Korean locale** — ko-KR, Asia/Seoul timezone
- **Domain-specific** — prefer purpose-built surfaces over generic layouts

## Surface Patterns

Choose a pattern based on content type:

| Pattern | Use Case | theme.pattern |
|---------|----------|-----------|
| Immersive (풀 캔버스) | weather, travel, daily | `immersive` |
| Side Panel (사이드 패널) | news, smart home, media, commute | `sidePanel` |
| Center Card (센터 카드) | schedule, finance, delivery, wellness | `centerCard` |
| Top Banner (상단 배너) | emergency, urgent weather, short status | `topBanner` |
| Bottom Ribbon (하단 리본) | sports ticker, media companion, reminders | `bottomRibbon` |

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
- Do NOT create more than ~40 components per surface

## Minimal Example

A simple card with a title and a value:

```
{"version": "v0.9", "createSurface": {"surfaceId": "weather_brief", "catalogId": "https://a2ui.org/specification/v0_9/standard_catalog.json", "theme": {"domain": "weather", "pattern": "centerCard"}}}
{"version": "v0.9", "updateDataModel": {"surfaceId": "weather_brief", "value": {"title": "서울 날씨", "temp": "18°", "condition": "맑음"}}}
{"version": "v0.9", "updateComponents": {"surfaceId": "weather_brief", "components": [{"id": "root", "component": "Column", "justify": "center", "children": ["heroCard"]}, {"id": "heroCard", "component": "Card", "child": "cardContent"}, {"id": "cardContent", "component": "Column", "children": ["titleText", "tempRow"]}, {"id": "titleText", "component": "Text", "text": {"path": "/title"}, "variant": "h3"}, {"id": "tempRow", "component": "Row", "justify": "spaceBetween", "children": ["tempText", "conditionText"]}, {"id": "tempText", "component": "Text", "text": {"path": "/temp"}, "variant": "h1"}, {"id": "conditionText", "component": "Text", "text": {"path": "/condition"}, "variant": "body"}]}}
```

## Full Examples

See `references/examples/` for production-validated A2UI JSON:
- `weather.json` — immersive weather briefing with hourly forecast
- `news.json` — side panel news with headline list
- `card_briefing.json` — center card financial snapshot
