# tv_fetch

Agent-friendly C++ CLI for fetching and normalizing TV domain context.

`tv_fetch` is the data-only layer for the OpenClaw TV pipeline:

```text
user request -> domain fetch -> normalized JSON -> A2UI composition -> viewer app
```

This project intentionally stops at **normalized domain JSON**. It does not
generate A2UI and it does not launch the viewer.

Current command coverage is split in two layers:

- Live-ready domains: `weather`, `news`, `finance`, `commute`, `sports`,
  `schedule`, `travel`, `emergency`, `daily`
- Mock-ready scenario parity for the remaining TV skills: `family`,
  `meal-delivery`, `media`, `shopping`, `smart-home`, `wellness`

For Tizen packaging, the repository includes the RPM spec at
[packaging/tv_fetch.spec](/Users/yohoho/work/tv_fetch/packaging/tv_fetch.spec).

## Design goals

- JSON-first output for humans and AI agents
- Self-describing commands via `describe`
- Deterministic normalized payloads for downstream A2UI composition
- Input hardening for agent-passed arguments
- Small dependency surface: `libcurl` + system `libjson`

The command surface takes cues from the sibling `../cli` project and from the
AI-agent CLI guidance in Justin Poehnelt's “Rewrite your CLI for AI agents”.

## Build

```bash
cd /Users/yohoho/work/tv_fetch
meson setup builddir
meson compile -C builddir
meson test -C builddir
```

If Meson and Ninja are not installed yet:

```bash
python3 -m pip install meson ninja
```

System library dependencies:

- `libcurl`
- `libjson` at runtime
- `libjson-devel` for builds (`pkg-config json`, with `json-c` fallback for non-Tizen dev boxes)

If `latitude/longitude` are omitted for `open-meteo`, `tv_fetch` resolves the
requested `city/district` through a geocoding API first.

## Tizen packaging

The project includes a spec-only native packaging path for Tizen. It builds
`tv_fetch` as a regular native binary package instead of a manifest-based app.

Typical GBS build entry point:

```bash
cd /Users/yohoho/work/tv_fetch
gbs build -A aarch64 --include-all
```

The RPM spec configures Meson with:

- `--prefix=/usr`
- `-Dfixture_root=/usr/share/tv_fetch`

That keeps the bundled mock fixture under `/usr/share/tv_fetch/fixtures/` and
lets the CLI load it correctly at runtime without a `tizen-manifest.xml`.

## Commands

### Describe the CLI

```bash
./builddir/tv_fetch describe --format pretty
./builddir/tv_fetch describe weather --format pretty
./builddir/tv_fetch describe finance --format pretty
./builddir/tv_fetch describe schedule --format pretty
```

### Fetch weather context

Mock payload:

```bash
./builddir/tv_fetch weather --source mock --format pretty
```

Live Open-Meteo payload:

```bash
./builddir/tv_fetch weather \
  --source open-meteo \
  --city 서울 \
  --district 중구 \
  --latitude 37.5665 \
  --longitude 126.9780 \
  --hours 6 \
  --format pretty
```

Dry run without network:

```bash
./builddir/tv_fetch weather --source open-meteo --dry-run --format pretty
```

### Fetch news context

Mock payload:

```bash
./builddir/tv_fetch news --source mock --format pretty
```

Live Yonhap RSS payload:

```bash
./builddir/tv_fetch news --source yonhap-rss --count 6 --format pretty
```

Search by keyword:

```bash
./builddir/tv_fetch news --query 반도체 --count 6 --format pretty
```

### Fetch finance context

Mock payload:

```bash
./builddir/tv_fetch finance --source mock --format pretty
```

Live KRW-first market payload:

```bash
./builddir/tv_fetch finance \
  --source naver-public \
  --watchlist '005930:삼성전자,000660:SK하이닉스,035420:NAVER' \
  --format pretty
```

### Fetch commute context

Mock payload:

```bash
./builddir/tv_fetch commute --source mock --format pretty
```

Live OSRM payload:

```bash
./builddir/tv_fetch commute \
  --source osrm \
  --origin '망포역' \
  --destination '서초구청' \
  --profile driving \
  --format pretty
```

### Fetch sports context

Mock payload:

```bash
./builddir/tv_fetch sports --source mock --format pretty
```

Live league payload:

```bash
./builddir/tv_fetch sports \
  --source thesportsdb \
  --league kleague1 \
  --format pretty
```

### Fetch schedule context

Mock payload:

```bash
./builddir/tv_fetch schedule --source mock --format pretty
```

Live ICS payload:

```bash
./builddir/tv_fetch schedule \
  --source ics-url \
  --ics-url https://holidays.hyunbin.page/basic.ics \
  --days 7 \
  --max-events 6 \
  --format pretty
```

### Fetch travel context

Mock payload:

```bash
./builddir/tv_fetch travel --source mock --format pretty
```

Live airport payload:

```bash
./builddir/tv_fetch travel \
  --source airport-kr \
  --flight-number KE913 \
  --terminal T2 \
  --date 20260315 \
  --format pretty
```

### Fetch emergency context

Mock payload:

```bash
./builddir/tv_fetch emergency --source mock --format pretty
```

Live KMA payload:

```bash
./builddir/tv_fetch emergency \
  --source kma-combined \
  --format pretty
```

### Fetch daily context

Mock payload:

```bash
./builddir/tv_fetch daily --source mock --format pretty
```

Live composed payload:

```bash
./builddir/tv_fetch daily \
  --source compose-live \
  --format pretty
```

### Fetch additional mock-ready scenario context

These commands mirror the remaining TV scenario skills so downstream agents can
always ask `tv_fetch` for a normalized payload, even when a live adapter is not
implemented yet.

```bash
./builddir/tv_fetch family --source mock --format pretty
./builddir/tv_fetch meal-delivery --source mock --format pretty
./builddir/tv_fetch media --source mock --format pretty
./builddir/tv_fetch shopping --source mock --format pretty
./builddir/tv_fetch smart-home --source mock --format pretty
./builddir/tv_fetch wellness --source mock --format pretty
```

Dry run works the same way for these scenario commands:

```bash
./builddir/tv_fetch emergency --dry-run --format pretty
./builddir/tv_fetch daily --source compose-live --dry-run --format pretty
```

## Output shape

Each command emits normalized JSON that stays close to the corresponding TV
skill contract. A few representative top-level shapes:

```json
{
  "domain": "weather",
  "source": "open-meteo",
  "location": {
    "city": "서울",
    "district": "중구"
  },
  "updated_at": "2026-03-18T07:00",
  "headline": "서울 현재 흐림, 체감 6°입니다.",
  "current": {
    "temperature_c": 8.4,
    "feels_like_c": 6.1,
    "condition": "흐림",
    "humidity_pct": 61,
    "precip_probability_pct": 30
  },
  "alert": {
    "level": "안내",
    "title": "공식 특보 연동 필요",
    "summary": "현재 화면은 Open-Meteo 실황과 예보를 사용합니다. 재난성 특보는 기상청 공식 채널과 별도로 연동하세요.",
    "source": "Open-Meteo",
    "issued_at": "2026-03-18T07:00"
  },
  "hourly": [],
  "footer": "실황과 예보는 Open-Meteo 기반이며, 특보는 공식 소스를 별도로 연결하는 편이 안전합니다."
}
```

```json
{
  "domain": "news|finance|commute|sports|daily|emergency|family|meal-delivery|media|schedule|shopping|smart-home|travel|wellness",
  "source": "mock|live-source",
  "title": "string",
  "headline": "string",
  "primaryMetrics": [],
  "sections": [],
  "alert": {},
  "actions": [],
  "footer": "string"
}
```

## Roadmap

- Add top-level `schema` command for output contracts
- Add stable machine-readable error codes across all domains
- Improve UTF-8-safe shortening and source-specific copy cleanup for a few live feeds
