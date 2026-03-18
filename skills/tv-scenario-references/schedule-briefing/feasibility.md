# Feasibility

- Phase: `P0`
- TV UX fit: High
- Live data difficulty: Medium
- Recommended surface: `split dashboard` or `side panel`
- Primary payload: `current item`, `next item`, `travel or prep buffer`, `conflicts`, `location summary`
- Core dependencies: `calendar adapter`, KST handling, consent rules, optional commute estimation
- Demo scope: Show today timeline, next meeting card, conflict banner, and prep countdown.
- Trust boundary: Mask attendee names, notes, and exact locations by default because the TV can be a shared screen.
- Live blockers: Calendar authentication, consent UX, and privacy defaults for public-room viewing.
- Korea-first notes: Use 24-hour time, Korean date labels, and workday-friendly grouping by morning, afternoon, and evening.
