# Feasibility

- Phase: `P2`
- TV UX fit: High
- Live data difficulty: High
- Recommended surface: `center card` or `bottom ribbon`
- Primary payload: `order progress`, `ETA`, `courier status`, `reorder or contact action`
- Core dependencies: `order-status API`, user identity, push-style refresh, merchant metadata
- Demo scope: Show an order timeline, latest status, ETA band, and one safe follow-up action.
- Trust boundary: Protect address and contact details, and avoid overstating ETA confidence when the source is noisy.
- Live blockers: Closed platform APIs, account linking, and high-PII payloads.
- Korea-first notes: Food delivery is common in Korea, so ETA clarity and neighborhood context matter more than verbose text.
