# Feasibility

- Phase: `P1`
- TV UX fit: High
- Live data difficulty: Medium to high
- Recommended surface: `side panel` or `center card`
- Primary payload: `next destination`, `travel time`, `recommended departure`, `traffic risk`, `alternate routes`
- Core dependencies: `calendar or destination source`, route API, place normalization, KST handling
- Demo scope: Show next appointment destination, travel ETA, leave-by time, and one alternate option.
- Trust boundary: Avoid precise home or office disclosure and make route freshness visible before showing departure advice.
- Live blockers: Personalized location integration and stale route guidance risk.
- Korea-first notes: Account for Seoul commute density and transit-first recommendations where appropriate.
