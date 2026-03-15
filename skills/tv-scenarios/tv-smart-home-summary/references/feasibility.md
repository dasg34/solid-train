# Feasibility

- Phase: `P2`
- TV UX fit: High
- Live data difficulty: Medium to high
- Recommended surface: `modular dashboard`
- Primary payload: `doors`, `lights`, `temperature`, `air quality`, `cameras`, `unusual alerts`
- Core dependencies: `home platform adapter`, device normalization, room grouping, refresh policy
- Demo scope: Show room-by-room state, one urgent anomaly, and a compact camera or sensor strip.
- Trust boundary: Keep rendering read-only by default and separate UI summaries from any command execution path.
- Live blockers: Device heterogeneity, account linking, and unsafe control coupling.
- Korea-first notes: Indoor air quality and seasonal humidity are especially useful for Korea-first household surfaces.
