# Feasibility

- Phase: `P2`
- TV UX fit: Medium to high
- Live data difficulty: High
- Recommended surface: `side panel`
- Primary payload: `cast info`, `episode guide`, `soundtrack`, `trivia`, `companion controls`
- Core dependencies: `playback context`, media metadata provider, rights-safe assets, spoiler policy
- Demo scope: Show a side companion card with cast, current episode context, and one safe interaction.
- Trust boundary: Respect content rights, avoid spoilers by default, and do not imply synchronized playback accuracy without proof.
- Live blockers: Now-playing integration, metadata licensing, and spoiler-control policy.
- Korea-first notes: Local and global streaming catalogs differ, so the metadata layer should stay provider-agnostic.
