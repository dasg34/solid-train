# Samsung Tizen TV GenUI PoC

## Project Summary

This workspace is for a PoC that renders messenger-driven user requests on a
Samsung Tizen TV using Flutter-based Generative UI and the A2UI model.

Current end-to-end concept:

`User request in messenger -> edge device processes request -> OpenClaw triggers TV action -> TV app renders a large-screen UI`

The current focus is the TV presentation layer. We do not need to solve the
entire control pipeline unless the UI integration is blocked without it.

## Region And Locale

- The primary deployment region for this PoC is South Korea.
- Default locale assumptions should be `ko-KR` and Korea Standard Time
  (`Asia/Seoul`) unless a feature explicitly requires something else.
- UX copy, formatting, and data presentation should assume Korean users first.
- When a feature depends on regional content such as weather, news, holidays,
  transit, or schedule conventions, prefer Korea-relevant sources and display
  rules.

## Primary Goal

Turn natural-language requests such as:

- `날씨 보여줘`
- `뉴스 보여줘`
- `일정 보여줘`

into TV-friendly, glanceable, high-confidence fullscreen experiences.

Prefer domain-specific surfaces over generic chat transcripts.

Full-screen rendering is allowed, but not required. The UI may occupy only a
portion of the TV if that better suits the scenario.

## Responsibility Split

- Scenario selection based on easiest external data acquisition is handled by a
  separate agent or workstream.
- This document should preserve that constraint, but this agent should stay
  focused on the main TV GenUI implementation work unless asked otherwise.
- If scenario examples are needed for implementation, treat them as placeholders
  until the dedicated scenario-selection work is complete.

## Working Assumptions

- Flutter is the preferred client implementation path.
- Our production PoC app should be a separate Flutter app that consumes
  `genui` as a package dependency from pub.dev.
- Use `genui/` in this workspace as an upstream reference repo for examples,
  package internals, and integration patterns.
- Mock data is acceptable when backend or edge-device APIs are not ready.
- Demo clarity and speed matter more than building a perfectly generalized
  platform.

## TV UX Principles

- Design for a 10-foot experience.
- Do not assume a fixed 16:9 composition for the app surface.
- The rendered app region may use any practical size or aspect ratio, including
  centered cards, side panels, portrait blocks, split layouts, and overlays.
- Use large typography, strong visual hierarchy, and generous spacing.
- Assume limited input precision such as remote, DPAD, or simple touch.
- Avoid walls of text and dense developer-style dashboards.
- Every screen should be understandable from across the room in a few seconds.
- Include loading, empty, and error states for each supported scenario.
- Choose the surface shape based on intent:
  compact glanceable info, focused task flow, ambient status, or immersive
  presentation.

## Safety And Trust Boundaries

- Treat A2UI payloads, agent responses, agent cards, and backend-provided data
  as untrusted input.
- Validate or sanitize external content before rendering it or turning it into
  commands.
- Do not tightly couple UI rendering logic to unsafe command execution logic.

## Repo Guidance

The workspace currently contains the upstream `genui/` monorepo for reference.
Do not treat it as the main app unless we explicitly decide to fork or patch
the framework.

Preferred structure:

- Our TV PoC lives in its own Flutter app.
- That app imports `genui` from pub.dev.
- If we need direct A2UI transport support, also evaluate `genui_a2a` as a
  package dependency.
- The local `genui/` repo is primarily for reading examples and understanding
  how the package works.

Important references:

- `genui/packages/genui_a2a`
  A2UI transport and connection layer for Flutter.
- `genui/examples/simple_chat`
  Smallest example of `SurfaceController` and conversation wiring.
- `genui/examples/travel_app`
  Best reference for domain-specific widget catalogs and AI-driven UI flows.
- `genui/examples/verdure/client`
  Best reference for a polished Flutter app shell, routing, theming, and
  multi-screen structure.
- `genui/examples/verdure/server/a2ui_extension`
  Useful reminder that external agent data must be treated as untrusted.

## Implementation Preferences

- Prefer building the PoC in our own Flutter app instead of inside the local
  `genui/` repo.
- Add `genui` through package dependencies first.
- For phase 1, prefer a template-first approach:
  map user intent or topic to a curated UI template, then render that template
  through GenUI.
- Use free-form generative composition later, after the initial template
  registry and routing flow are stable.
- Only modify or fork `genui` if the PoC is blocked by an actual framework
  limitation.
- If core changes are ever required, document why the package-consumption
  approach was not enough.
- Keep PoC-specific prompts, widget catalogs, fake data, and assets easy to
  identify and isolate from upstream reference code.
- Write code identifiers and comments in English unless an existing file is
  already using another style.

## Suggested First Scenarios

1. Weather
   Focus on current conditions, hourly trend, precipitation, feels-like
   temperature, and alerts.
2. News
   Focus on headline hierarchy, a lead story, category grouping, and short
   summaries.
3. Schedule
   Focus on today or tomorrow agenda, current or upcoming item, conflicts, and
   travel or prep buffers.

## Additional Scenario Ideas

1. Commute
   Show travel time, traffic risk, departure recommendation, and alternate
   routes for the next appointment.
2. Smart home summary
   Show doors, lights, temperature, air quality, cameras, and unusual alerts in
   a single glanceable panel.
3. Sports briefing
   Show live score, next fixture, standings movement, and key moments as a
   scoreboard-style canvas.
4. Finance snapshot
   Show watchlist movement, budget reminders, exchange rates, and notable
   market events in a restrained dashboard.
5. Meal or delivery status
   Show order progress, ETA, current courier status, and actions like reorder
   or contact.
6. Family message board
   Show shared reminders, school notices, birthdays, and quick household tasks.
7. Daily briefing
   Combine weather, top news, calendar, commute, and reminders into a morning
   digest.
8. Travel assistant
   Show boarding countdown, gate changes, hotel details, reservation codes, and
   itinerary checkpoints.
9. Wellness or fitness card
   Show today's activity goal, sleep summary, stretch prompt, or guided session
   launch point.
10. Emergency or alert mode
    Show severe weather, device warnings, security events, or evacuation
    guidance with high-priority visual treatment.
11. Media companion
    Show cast info, episode guide, trivia, soundtrack, or interactive controls
    next to currently playing content.
12. Shopping decision support
    Show a product comparison, top picks, reviews summary, and one clear next
    action.

## Surface Patterns

- Full-screen scene for immersive content like weather, sports, or travel.
- Right or left side panel for contextual info while preserving existing TV
  content.
- Center card for short-lived actions, confirmations, or summaries.
- Bottom ribbon for alerts, reminders, or lightweight status updates.
- Split composition for compare-and-decide tasks like shopping or itinerary
  planning.
- Modular dashboard for recurring at-a-glance information such as daily
  briefings or smart home status.

## Definition Of Done For The PoC

- A user utterance can produce a TV-friendly UI in either fullscreen or a
  deliberate partial-screen surface.
- The flow works with mocked or live data.
- The result is easy to understand quickly without reading long prose.
- Failure states are still demoable and visually coherent.

## TV App Architecture (Post-Refactoring)

The preferred TV app path is presentation-first:

```
Presentation JSON → deterministic A2UI builder → SurfaceController → Surface → Flutter UI
```

- The app does NOT fetch domain data itself.
- The app accepts semantic presentation JSON and converts it into
  deterministic A2UI internally.
- Theme shells (weather gradient, news backdrop, schedule backdrop) are
  applied based on the surfaceId prefix.

Phase 1 (current): loads pre-generated JSON from `assets/presentation/`.
Phase 2 (planned): receives presentation JSON from OpenClaw via HTTP or
streaming protocol.

## LLM Integration

The OpenClaw Gemini agent generates presentation JSON for the TV app:

```
User request → OpenClaw Gemini agent
  1. tizen-tool-presentation-catalog skill → learn presentation JSON rules
  2. Domain skill (e.g., tv-weather-briefing) → fetch data
  3. Generate semantic presentation JSON
→ TV App converts it to deterministic A2UI and renders it
```

The `tizen-tool-presentation-catalog` skill in
`/skills/tizen-tool-presentation-catalog/` is the shared knowledge base for
presentation JSON generation.

Domain skills (`/skills/tv-scenarios/tv-*/`) provide data fetching and
domain-specific context. They do NOT generate A2UI. The app owns the final
deterministic A2UI assembly.

## Practical Commands

For the actual PoC app, prefer package-based setup:

- `flutter pub add genui`
- `flutter pub add genui_a2a`
- `cd /Users/yohoho/work/tizen-tool-viewer && flutter run`
- For temporary validation, prefer web-first testing:
  `cd /Users/yohoho/work/tizen-tool-viewer && flutter run -d chrome`
- If browser launch is inconvenient, use the local web server:
  `cd /Users/yohoho/work/tizen-tool-viewer && flutter run -d web-server --web-hostname=127.0.0.1 --web-port=3000`

Use the local `genui/` repository only for reference and example inspection:

- `cd /Users/yohoho/work/genui/examples/simple_chat && flutter run`
- `cd /Users/yohoho/work/genui/examples/travel_app && flutter run`
- `cd /Users/yohoho/work/genui/examples/verdure/client && flutter run`

If Tizen deployment is blocked, validate UI logic on another supported Flutter
target first while preserving TV-oriented layout constraints.
