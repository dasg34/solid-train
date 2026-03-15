# Live Data Notes

- Verified live TheSportsDB access on March 15, 2026.
- Default Korean league presets:
  - `kleague1` -> league ID `4689`
  - `kleague2` -> league ID `4822`
  - `kbo` -> league ID `4830`
- The skill can emit A2UI directly from live sports feeds with:
  `python3 scripts/generate_sports_a2ui.py --source thesportsdb --league kleague1`
- TheSportsDB is a practical no-key bridge for PoC work, but stale-data handling and explicit refresh timestamps remain important.
