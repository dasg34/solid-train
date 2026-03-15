# Feasibility

- Phase: `P1`
- TV UX fit: High
- Live data difficulty: Medium to high
- Recommended surface: `fullscreen scene` or `split composition`
- Primary payload: `boarding countdown`, `gate changes`, `hotel details`, `reservation codes`, `checkpoints`
- Core dependencies: `itinerary source`, flight-status feed, reservation model, timezone conversion
- Demo scope: Show a trip hero card, next checkpoint, reservation summary, and one disruption banner.
- Trust boundary: Mask booking codes until needed and show last-updated timestamps whenever operational data can change.
- Live blockers: Reservation integration breadth and late operational changes from airlines or hotels.
- Korea-first notes: Outbound travel flows often center on airport countdowns and transfer checkpoints, which fit TV well.
