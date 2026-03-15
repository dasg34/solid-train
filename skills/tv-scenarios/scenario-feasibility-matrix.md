# TV Scenario Feasibility Matrix

This matrix consolidates the parallel scenario review for the Samsung Tizen TV GenUI PoC.

| Scenario | Phase | TV fit | Live data | Recommended surface |
| --- | --- | --- | --- | --- |
| `tv-weather-briefing` | `P0` | High | Low to medium | `fullscreen scene` |
| `tv-news-briefing` | `P0` | High | Medium | `fullscreen scene` or `side panel` |
| `tv-schedule-briefing` | `P0` | High | Medium | `split dashboard` or `side panel` |
| `tv-commute-briefing` | `P1` | High | Medium to high | `side panel` or `center card` |
| `tv-smart-home-summary` | `P2` | High | Medium to high | `modular dashboard` |
| `tv-sports-briefing` | `P0` | Very high | Medium | `fullscreen scoreboard canvas` |
| `tv-finance-snapshot` | `P2` | Medium | Medium to high | `restrained dashboard` |
| `tv-meal-delivery-status` | `P2` | High | High | `center card` or `bottom ribbon` |
| `tv-family-message-board` | `P1` | High | Medium | `modular dashboard` or `center card` |
| `tv-daily-briefing` | `P1` | Very high | High | `modular dashboard` |
| `tv-travel-assistant` | `P1` | High | Medium to high | `fullscreen scene` or `split composition` |
| `tv-wellness-card` | `P2` | Medium | Medium to high | `center card` or `ambient status` |
| `tv-emergency-alert-mode` | `P1` | Very high | Medium | `fullscreen alert` or `bottom ribbon` |
| `tv-media-companion` | `P2` | Medium to high | High | `side panel` |
| `tv-shopping-decision-support` | `P1` | Medium to high | Medium | `split comparison` |

## Recommended Rollout

- `P0`: `tv-weather-briefing`, `tv-news-briefing`, `tv-schedule-briefing`, `tv-sports-briefing`
- `P1`: `tv-daily-briefing`, `tv-commute-briefing`, `tv-family-message-board`, `tv-travel-assistant`, `tv-emergency-alert-mode`, `tv-shopping-decision-support`
- `P2`: `tv-smart-home-summary`, `tv-finance-snapshot`, `tv-meal-delivery-status`, `tv-wellness-card`, `tv-media-companion`

## Local Reference Stack

- `simple_chat`: smallest baseline for `SurfaceController` and conversation wiring
- `travel_app`: strongest reference for domain-specific widget catalogs and tool-backed mock/live flows
- `verdure/client`: strongest reference for polished app shell, routing, and presentation screens
- `genui_a2a`: optional only when direct A2A transport is necessary for the PoC

## Selection Notes

- Prioritize single-domain, high-confidence surfaces before composite dashboards.
- Treat all external agent and backend payloads as untrusted input.
- Keep every scenario demoable with mock data before betting on live integrations.
