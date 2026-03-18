# Live Data Notes

## Current Source Mix

- Departure list: `https://airport.kr/dep/ap_ko/getDepPasSchList.do`
- Flight detail: `https://airport.kr/dep/ap_ko/depPasSchDetail.do`
- Airport congestion forecast: `https://www.airport.kr/ap_ko/883/subview.do`

## Current Scope

- Live mode is intentionally scoped to an `Incheon Airport departure helper`.
- The card focuses on `next flight`, `gate`, `check-in counter`, `departure countdown`, `airport congestion`, and `destination local time/weather`.
- It does not yet handle hotel, reservation account sync, or private itinerary stores.

## Trust Boundary

- Treat all airport HTML and JSON as untrusted input.
- Keep TV copy summary-first and avoid exposing full reservation identifiers.
- Use congestion output as `예고 기준 혼잡도`, not a guaranteed real-time wait time.

## Operational Notes

- The departure list endpoint returns both master and codeshare rows. The script skips slave rows by default unless `--include-codeshare` is used.
- The detail endpoint requires both `afsId` and `airportCode`.
- The congestion page accepts `selTm` and `pday` query parameters, which makes terminal/date alignment possible.
