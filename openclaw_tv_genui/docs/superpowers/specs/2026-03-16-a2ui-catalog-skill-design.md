# A2UI Catalog Skill for OpenClaw LLM

## Summary

Create a shared skill (`tv-a2ui-catalog`) that teaches the OpenClaw Gemini
agent how to generate valid A2UI JSON for the TV app. The skill contains
catalog component definitions, generation rules, TV UX principles, and
reference examples.

### Current State

- TV app (`openclaw_tv_genui`) can render any valid A2UI JSON via
  `A2uiPayloadSource` → `SurfaceController` → `Surface`.
- OpenClaw agent (Gemini) exists but does not know how to produce A2UI JSON.
- Skills (`/skills/tv-scenarios/`) have Python scripts that generate A2UI via
  hardcoded templates. These serve as good reference examples.

### Target State

```
User: "날씨 보여줘"
→ OpenClaw Gemini agent
  1. tv-a2ui-catalog skill 참조 → A2UI 생성법 습득
  2. tv-weather-briefing skill → 날씨 데이터 획득
  3. 카탈로그 부품으로 A2UI JSON 조립
→ TV App: 렌더링
```

## Prerequisite: Register Button in TV App Catalog

The TV app currently registers 6 components (Card, Column, Divider, Icon, Row,
Text) but not Button. The existing skill-generated JSON files all emit Button
components. Add `BasicCatalogItems.button` to the `_createController()` method
in `genui_scenario_surface.dart` before proceeding.

## Approach: Prompt-Centric Skill

The skill uses natural language + examples (not JSON Schema) to teach the LLM.
This matches the verdure example's approach and is more effective for LLM
consumption than formal schemas.

## Skill Structure

```
/skills/tv-a2ui-catalog/
├── SKILL.md                    — A2UI generation guide for the LLM
├── agents/openai.yaml          — skill metadata
└── references/
    └── examples/
        ├── weather.json        — immersive pattern example
        ├── news.json           — sidePanel pattern example
        └── card_briefing.json  — centerCard pattern example
```

## SKILL.md Content

### Section 1: A2UI Protocol

Three messages in this order:

1. `createSurface` — surface 생성
   - `surfaceId`: unique identifier (e.g., `"weather_main"`)
   - `catalogId`: always
     `"https://a2ui.org/specification/v0_9/standard_catalog.json"`
2. `updateDataModel` — 데이터 주입 (optional if no data bindings)
   - `surfaceId`: must match createSurface
   - `value`: flat key-value map of all dynamic data
3. `updateComponents` — 부품 트리 정의
   - `surfaceId`: must match createSurface
   - `components`: array of component objects

Each message wrapped in `{"version": "v0.9", ...}`.
Output format: NDJSON (one JSON object per line).

Ordering is a convention. `updateDataModel` may be omitted for purely static
surfaces. The framework processes messages independently.

### Section 2: Available Components

The TV app registers these 7 components. The LLM must only use these.

Every component must have:
- `id` (unique string) — required
- `component` (type name) — required
- `weight` (integer, optional) — controls flex sizing inside Row/Column

One component must have `id: "root"` — this is the render entry point.

| Component | Required Properties | Optional Properties |
|-----------|-------------------|-------------------|
| Text | `text` (path or literal) | `variant` (h1/h2/h3/h4/h5/body/caption) |
| Column | `children` (id array) | `justify`, `align` |
| Row | `children` (id array) | `justify`, `align` |
| Card | `child` (id) | — |
| Icon | `name` (see valid list below) | — |
| Divider | — | `axis` (horizontal/vertical) |
| Button | `child` (id), `action` | `variant` (primary/borderless) |

#### Valid Icon Names

Only these icon names are supported. Using any other name will render a broken
image icon:

```
accountCircle, add, arrowBack, arrowForward, attachFile, calendarToday, call,
camera, check, close, delete, download, edit, error, event, favorite,
favoriteOff, folder, help, home, info, locationOn, lock, lockOpen, mail, menu,
moreHoriz, moreVert, notifications, notificationsOff, payment, person, phone,
photo, print, refresh, search, send, settings, share, shoppingCart, star,
starHalf, starOff, upload, visibility, visibilityOff, warning
```

#### Dynamic-Length Lists (Template Children)

Column, Row, and List support template-based children for rendering
variable-length data from the data model:

```json
{
  "id": "hourlyList",
  "component": "Column",
  "children": {"componentId": "hourlyRow", "path": "/hourlyItems"}
}
```

This renders one `hourlyRow` component per item in the `hourlyItems` array.
Use this for lists of unknown length (forecast hours, news articles, etc.).

### Section 3: Data Binding

Dynamic values reference the dataModel:
```json
{"text": {"path": "/currentTemp"}}
```

Static values are literal:
```json
{"text": "서울 강남구"}
```

All dynamic data must be defined in the `updateDataModel` message's `value`
map. Paths use `/key` format (single level, no nesting).

### Section 4: TV UX Principles

- Design for a 10-foot experience — understandable from across the room
- Large typography, strong visual hierarchy, generous spacing
- No walls of text, no dense dashboards
- Every screen glanceable in a few seconds
- Keep under ~40 components per surface for glanceability
- Korean locale (ko-KR), Korean Standard Time (Asia/Seoul)
- Prefer domain-specific surfaces over generic chat transcripts

### Section 5: Surface Patterns

| Pattern | Use Case | surfaceId Convention |
|---------|----------|---------------------|
| Immersive (풀 캔버스) | weather, sports, travel | `{domain}_main` |
| Side Panel (사이드 패널) | news, smart home, media | `{domain}_main` |
| Center Card (센터 카드) | schedule, finance, delivery | `{domain}_main` |

The TV app resolves the theme shell from the surfaceId prefix:
- `weather_*` → atmospheric weather theme
- `news_*` → news panel theme
- `schedule_*` → schedule panel theme
- everything else → standard dark theme

### Section 6: Common Mistakes

- Do NOT nest `components` inside `createSurface`
- Do NOT reference a data path not defined in `updateDataModel`
- Do NOT use component IDs not defined in the components array
- Do NOT forget the `"root"` component
- Do NOT use icon names outside the allowed list above
- Do NOT create more than ~40 components in a single surface

### Section 7: Examples

SKILL.md includes one minimal inline example (5-10 components) showing the
complete 3-message flow. For full examples, reference `references/examples/`:

- `weather.json` — immersive pattern with rich data binding
- `news.json` — side panel pattern with list structure
- `card_briefing.json` — center card pattern with generic card layout

## Example Files

Copy from the TV app's generated assets (`openclaw_tv_genui/assets/a2ui/`):
- `weather.json` → `references/examples/weather.json`
- `news.json` → `references/examples/news.json`
- Pick one card briefing (e.g., `finance.json`) →
  `references/examples/card_briefing.json`

**Important:** Audit all example files for invalid icon names before copying.
Replace invalid icons with valid ones from the allowed list. The existing
skill-generated JSON files use icon names that are not in the genui
`AvailableIcons` enum.

## README Updates

### AGENTS.md (workspace root)

Add a section documenting:
- Post-refactoring TV app architecture (thin A2UI renderer)
- LLM integration direction (OpenClaw agent + tv-a2ui-catalog skill)
- Skills structure (tv-a2ui-catalog as shared A2UI knowledge base)

### openclaw_tv_genui/README.md (new file)

Brief app README covering:
- What the app does (renders A2UI JSON on TV)
- Current architecture (A2uiPayloadSource → SurfaceController → Surface)
- How to run (`flutter run -d chrome` or web-server)
- File structure overview
- Registered catalog components (7: Text, Column, Row, Card, Icon, Divider,
  Button)
- How A2UI JSON flows into the app

## Out of Scope

- OpenClaw agent implementation/modification
- Transport protocol (WebSocket, SSE, A2A) between OpenClaw and TV app
- Custom catalog components beyond BasicCatalogItems (Image, List, Tabs, etc.
  may be added later but are not part of this spec)
