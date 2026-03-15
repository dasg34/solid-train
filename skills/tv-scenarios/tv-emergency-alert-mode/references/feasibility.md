# Feasibility

- Phase: `P1`
- TV UX fit: Very high
- Live data difficulty: Medium
- Recommended surface: `fullscreen alert` or `bottom ribbon`
- Primary payload: `alert type`, `affected area`, `recommended action`, `expiry`, `confidence`
- Core dependencies: `official alert feed`, severity model, interrupt policy, acknowledgement state
- Demo scope: Show one severe alert state, one lower-priority ribbon state, and explicit no-alert fallback.
- Trust boundary: Never fabricate or amplify emergency content. Prefer omission over uncertain advice and always keep the source visible.
- Live blockers: Official-source access, interrupt rules, and liability-sensitive copy review.
- Korea-first notes: Korea-first alerting should align with KMA or other official channels and use unambiguous Korean action text.
