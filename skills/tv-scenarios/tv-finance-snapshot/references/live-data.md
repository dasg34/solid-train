# TV Finance Snapshot Live Data

## Default Live Path

- `watchlist + domestic indices`: Npay Securities polling endpoint
- `USD/KRW`, `100JPY/KRW`: Frankfurter API using ECB-based rates
- Locale baseline: `ko-KR`
- Time zone baseline: `Asia/Seoul`

## Commands

Run from `tv-finance-snapshot/`.

```bash
python3 scripts/generate_finance_a2ui.py --source naver-public
python3 scripts/generate_finance_a2ui.py --source naver-public --watchlist '005930:삼성전자,000660:SK하이닉스,035420:NAVER'
python3 scripts/generate_finance_a2ui.py --source naver-public --watchlist '005930:삼성전자,005380:현대차,035720:카카오'
python3 scripts/generate_finance_a2ui.py --source naver-public --dump-normalized /tmp/tv-finance-live.json
```

## Source Notes

- Npay Securities polling is suitable for Korea-first demo snapshots because it exposes compact domestic index and stock quote payloads without requiring account auth.
- Frankfurter provides ECB-based exchange-rate snapshots. The current adapter derives `USD/KRW` and `100JPY/KRW` by converting from the KRW base series and comparing with the previous available business day.
- Financial copy should stay descriptive, not prescriptive. Avoid wording that sounds like a recommendation, ranking, or buy/sell prompt.
- When any quote source fails, prefer a degraded alert over hiding the entire card, but keep the failure text short enough for TV reading distance.

## Trust Boundary

- Treat all finance payloads as untrusted external input.
- Keep source attribution visible in the footer or alert meta.
- Preserve date or market-status context such as `장중`, `장마감`, or ECB rate date so viewers can judge freshness correctly.
