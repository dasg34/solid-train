# TV Emergency Alert Mode Live Data

## Default Live Path

- `--source kma-combined`
- Primary source: KMA special-report page
- Fallback source: KMA domestic earthquake table
- Locale baseline: `ko-KR`
- Time zone baseline: `Asia/Seoul`

## Commands

Run from `tv-emergency-alert-mode/`.

```bash
python3 scripts/generate_emergency_alert_a2ui.py --source kma-combined
python3 scripts/generate_emergency_alert_a2ui.py --source kma-special-report
python3 scripts/generate_emergency_alert_a2ui.py --source kma-earthquake --min-magnitude 3.0 --max-age-days 7
```

## Source Rules

- `kma-combined` checks official KMA `특보`, `예비특보`, `속보`, and `기상정보` options first.
- If there is no current special-report item worth showing, it falls back to recent KMA earthquake entries above the configured threshold.
- If neither source yields a valid item, the skill emits the empty state.

## Trust Notes

- The TV surface should summarize official KMA information, not replace the original report or 재난문자.
- Thresholds such as `--min-magnitude` and `--max-age-days` should stay conservative so the screen does not overstate stale events.
