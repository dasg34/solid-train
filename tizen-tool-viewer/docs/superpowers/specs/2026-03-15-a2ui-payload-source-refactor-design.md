# A2UI Payload Source Refactoring

## Summary

Refactor the TV app from self-contained presenter/component generation to a
thin rendering client that consumes A2UI JSON messages produced by external
OpenClaw skills.

### Current State

The TV app (`openclaw_tv_genui`) owns the entire pipeline:

```
Prompt → TemplateRegistry.route() → Presenter.load()
       → hardcoded Component tree + dataModel → A2UI rendering
```

Presenters fetch live data (Open-Meteo, Yonhap RSS, ICS), build component
trees in Dart, and feed them to `A2uiMessageProcessor`.

### Target State

OpenClaw skills (`/skills/tv-scenarios/`) already generate A2UI JSON via Python
scripts. The TV app should consume that JSON and render only.

```
OpenClaw skill → A2UI JSON (NDJSON) → TV App → SurfaceController → render
```

## Prerequisite: genui Package Upgrade

The app currently depends on `genui: ^0.7.0` and `genui_a2ui: ^0.7.0`. The
skills output A2UI v0.9 JSON, which requires genui 0.8.0+ to parse. This is a
breaking upgrade:

- `A2uiMessageProcessor` → `SurfaceController`
- `GenUiSurface` → `Surface`
- `SurfaceUpdate` / `DataModelUpdate` / `BeginRendering` → `CreateSurface` /
  `UpdateDataModel` / `UpdateComponents`
- Component format: key-based → flat discriminator (`"component": "Text"`)
- Property renames: `distribution` → `justify`, `alignment` → `align`,
  `usageHint` → `variant`
- `genui_a2ui` → `genui_a2a` (package rename)

This upgrade must happen before or as part of this refactoring. See
`genui/docs/usage/migration/migration_0.7.0_to_0.9.0.md` for details.

## Approach: Hybrid (JSON files → HTTP)

- **Phase 1**: Pre-generate A2UI JSON from skills, load as local files in the
  TV app. Validates the rendering pipeline end-to-end.
- **Phase 2**: Replace the file source with an HTTP endpoint. Same interface,
  different source.

## Architecture

### A2uiPayloadSource Interface

```dart
abstract class A2uiPayloadSource {
  Future<List<A2uiMessage>> load(String scenarioId);
}
```

### Phase 1 Implementation: JsonFilePayloadSource

- Reads NDJSON files from `assets/a2ui/{scenarioId}.json`
- Splits lines, parses each via `A2uiMessage.fromJson()`
- Returns `List<A2uiMessage>`

### Phase 2 Implementation: HttpPayloadSource (future)

- Fetches from `GET /scenarios/{scenarioId}`
- Same NDJSON parsing logic
- Drop-in replacement for `JsonFilePayloadSource`

## A2UI JSON Consumption Flow

Skills output NDJSON (typically 3 messages per scenario, though the parser
handles any count):

```
{"version":"v0.9","createSurface":{"surfaceId":"weather_main","catalogId":"..."}}
{"version":"v0.9","updateDataModel":{"surfaceId":"weather_main","value":{...}}}
{"version":"v0.9","updateComponents":{"surfaceId":"weather_main","components":[...]}}
```

TV app processing:

```
NDJSON file → line split → A2uiMessage.fromJson() per line
            → SurfaceController.handleMessage() per message
            → Surface renders Flutter widgets
```

Skills JSON format is compatible with genui 0.8.0+ `A2uiMessage.fromJson()`.
No translation layer needed (after the prerequisite upgrade).

### Error Handling

`A2uiPayloadSource.load()` throws on failure (file not found, malformed JSON).
The consuming widget catches errors and displays an error state. Individual
message parse failures via `A2uiValidationException` are caught by
`SurfaceController` internally.

## Theme/Style Mapping

The `createSurface` message contains `surfaceId`. The consuming widget
inspects the first `A2uiMessage` (always `CreateSurface`), extracts
`surfaceId`, and resolves the theme shell before forwarding messages to
`SurfaceController`:

```dart
final style = resolveSurfaceStyle(messages.first); // inspect CreateSurface
// then feed all messages to SurfaceController
```

| surfaceId pattern     | TemplateSurfaceStyle  |
|-----------------------|-----------------------|
| `weather_*`           | atmosphericWeather    |
| `news_*`              | newsPanel             |
| `schedule_*`          | schedulePanel         |
| everything else       | standard              |

## HomeScreen Changes

### Before

```
Prompt input → TemplateRegistry.route() (keyword matching)
             → Presenter.load() → component generation → rendering
```

### After

```
Scenario list selection → A2uiPayloadSource.load(scenarioId)
                        → A2uiMessage stream → SurfaceController → rendering
```

### ScenarioEntry (replaces TemplateScenario)

```dart
class ScenarioEntry {
  final String id;                    // "weather", "news", etc.
  final String title;                 // "날씨 브리핑"
  final String summary;               // "현재 날씨, 시간별 예보, 체감 온도"
  final String surfaceId;             // "weather_main" — for style mapping
  final TvSurfacePattern pattern;     // immersive / sidePanel / centerCard
}
```

- Prompt routing logic removed (OpenClaw responsibility)
- Scenario selection is a simple list UI for PoC demo (shows title + summary)
- Scenario catalog defined as a const list in `scenario_entry.dart`

## File Changes

### Delete

```
lib/features/weather/data/weather_briefing_repository.dart
lib/features/news/data/news_briefing_repository.dart
lib/features/schedule/data/schedule_briefing_repository.dart
lib/features/card_briefing/data/mock_card_briefing_presenter.dart
lib/features/home/models/template_registry.dart
lib/features/home/models/template_scenario.dart
lib/features/home/models/template_surface.dart
```

### Create

```
lib/core/a2ui/a2ui_payload_source.dart         — abstract interface
lib/core/a2ui/json_file_payload_source.dart     — Phase 1: file-based loader
assets/a2ui/*.json                              — pre-generated NDJSON per scenario
lib/features/home/models/scenario_entry.dart    — lightweight scenario metadata
```

### Modify

```
lib/features/home/home_screen.dart              — simplify to list selection + payload source
lib/features/home/widgets/genui_scenario_surface.dart — rewrite to use SurfaceController
lib/app/openclaw_tv_app.dart                    — inject A2uiPayloadSource instead of TemplateRegistry
```

### Keep

```
lib/main.dart
lib/core/theme/app_theme.dart
```

Note: The themed shell widgets (weather gradient, news backdrop, schedule
backdrop) currently in `genui_scenario_surface.dart` should be preserved within
the rewritten surface widget.

### Dependencies

```
pubspec.yaml:
  genui: ^0.7.0 → upgrade to 0.8.0+ (A2UI v0.9 support)
  genui_a2ui: ^0.7.0 → remove (not needed for Phase 1 file-based loading)
```

`genui_a2a` (the renamed package) can be added later in Phase 2 if A2A
transport is chosen.

## Skills ↔ TV App Mapping

| Deleted TV App Code                        | Corresponding Skill                                                |
|--------------------------------------------|--------------------------------------------------------------------|
| WeatherTemplatePresenter + OpenMeteo repo  | tv-scenarios/tv-weather-briefing/scripts/generate_weather_a2ui.py  |
| NewsTemplatePresenter + Yonhap repo        | tv-scenarios/tv-news-briefing/scripts/generate_news_a2ui.py       |
| ScheduleTemplatePresenter + ICS repo       | tv-scenarios/tv-schedule-briefing/scripts/generate_schedule_a2ui.py|
| MockCardBriefingPresenter (11 scenarios)   | tv-scenarios/tv-{domain}/scripts/generate_{domain}_a2ui.py        |
| buildXxxComponents() functions             | _shared/scenario_a2ui.py build_scenario_messages()                |
| TemplateRegistry.route()                   | OpenClaw (future)                                                  |

## Asset Generation

To populate `assets/a2ui/`, run each skill script with mock data:

```bash
cd /Users/yohoho/work/skills/tv-scenarios
python3 tv-weather-briefing/scripts/generate_weather_a2ui.py \
  --state scenario --source mock > ../../openclaw_tv_genui/assets/a2ui/weather.json
```

Repeat for each scenario. This can be scripted.

## Out of Scope

- OpenClaw ↔ TV App transport protocol (WebSocket, SSE, A2A)
- Prompt routing / NLU (stays in OpenClaw)
- HttpPayloadSource implementation (Phase 2)
- Modifications to skills Python scripts
