# TV Commute Briefing Live Data

## Default Live Path

- `--source osrm`
- Geocoding: OpenStreetMap Nominatim
- Routing: OSRM public demo server
- Locale baseline: `ko-KR`
- Time zone baseline: `Asia/Seoul`

## Commands

Run from `tv-commute-briefing/`.

```bash
python3 scripts/generate_commute_a2ui.py --source osrm --origin "서울시청" --destination "강남역"
python3 scripts/generate_commute_a2ui.py --source osrm --origin "서울시청" --destination "강남역" --arrive-by 2026-03-16T09:00:00+09:00
python3 scripts/generate_commute_a2ui.py --source osrm --origin "서울시청" --destination "강남역" --profile walking
```

## Source Notes

- The current adapter estimates route distance and duration from a public routing engine. It does not include live traffic congestion or transit delays.
- Exact address strings should be masked or shortened before TV rendering when the location is sensitive.
- If the arrival target is many hours away, the summary should stay short instead of showing minute-precision long waits.

## Output Shape

The live adapter emits the same normalized scenario schema as the mock path:

- `primaryMetrics[]`
- `sections[]`
- `alert`
- `actions[]`
- `footer`
