# Feasibility

- Phase: `P1`
- TV UX fit: Very high
- Live data difficulty: High
- Recommended surface: `modular dashboard`
- Primary payload: `weather`, `top news`, `next schedule`, `commute`, `reminders`
- Core dependencies: Multiple domain adapters, prioritization logic, tile-level fallback, freshness policy
- Demo scope: Compose 3-4 stable cards from weather, news, and schedule before adding commute or alerts.
- Trust boundary: Expect partial failure and never hide stale or missing tiles behind a polished summary veneer.
- Live blockers: Cross-source orchestration, compounded trust errors, and privacy leakage across combined panels.
- Korea-first notes: Morning household usage in Korea favors a fast, glanceable digest rather than a long conversational flow.
