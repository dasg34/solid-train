# tizen-tool-domain-fetch

Agent-friendly C++ CLI for fetching and normalizing TV domain context.

`tizen-tool-domain-fetch` is the data-only layer for the OpenClaw TV pipeline:

```text
user request -> domain fetch -> normalized JSON -> presentation composition -> viewer app
```

This project intentionally stops at **normalized domain JSON**. It does not
generate presentation payloads and it does not launch the viewer.

Current command coverage is split in two layers:

- Live-ready domains: `weather`, `news`, `youtube`, `finance`, `commute`, `sports`,
  `schedule`, `travel`, `emergency`, `daily`
- Mock-ready scenario parity for the remaining TV skills: `family`,
  `meal-delivery`, `media`, `shopping`, `smart-home`, `wellness`

For Tizen packaging, the repository includes the RPM spec at
[packaging/tizen-tool-domain-fetch.spec](/Users/yohoho/work/tizen-tool-domain-fetch/packaging/tizen-tool-domain-fetch.spec).

## Design goals

- JSON-first output for humans and AI agents
- Self-describing commands via `describe`
- Deterministic normalized payloads for downstream presentation composition
- Input hardening for agent-passed arguments
- Small dependency surface: `libcurl` + system `libjson`

The command surface takes cues from the sibling `../cli` project and from the
AI-agent CLI guidance in Justin Poehnelt's “Rewrite your CLI for AI agents”.

## Build

```bash
cd /Users/yohoho/work/tizen-tool-domain-fetch
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

If `latitude/longitude` are omitted for `open-meteo`, `tizen-tool-domain-fetch` resolves the
requested `city/district` through a geocoding API first.

## Tizen packaging

The project includes a native RPM packaging path for Tizen. It builds
`tizen-tool-domain-fetch` as a regular native binary package, with:

- `packaging/tizen-tool-domain-fetch.spec` for the RPM build rules
- `packaging/tizen-tool-domain-fetch.manifest` as the GBS request-domain manifest

This is separate from an application-level `tizen-manifest.xml`.

Typical GBS build entry point:

```bash
cd /Users/yohoho/work/tizen-tool-domain-fetch
gbs build -A aarch64 --include-all
```

The RPM spec configures Meson with:

- `--prefix=/usr`
- `-Dfixture_root=/usr/share/tizen-tool-domain-fetch`

That keeps the bundled mock fixture under `/usr/share/tizen-tool-domain-fetch/fixtures/` and
lets the CLI load it correctly at runtime without a `tizen-manifest.xml`.

## Commands

### Describe the CLI

```bash
./builddir/tizen-tool-domain-fetch describe --format pretty
./builddir/tizen-tool-domain-fetch describe weather --format pretty
./builddir/tizen-tool-domain-fetch describe finance --format pretty
./builddir/tizen-tool-domain-fetch describe schedule --format pretty
```

### Fetch weather context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch weather --source mock --format pretty
```

Live Open-Meteo payload:

```bash
./builddir/tizen-tool-domain-fetch weather \
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
./builddir/tizen-tool-domain-fetch weather --source open-meteo --dry-run --format pretty
```

### Fetch news context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch news --source mock --format pretty
```

Live Yonhap RSS payload:

```bash
./builddir/tizen-tool-domain-fetch news --source yonhap-rss --count 6 --format pretty
```

Search by keyword:

```bash
./builddir/tizen-tool-domain-fetch news --query 반도체 --count 6 --format pretty
```

### Search YouTube videos

YouTube Data API v3 requires an API key:

```bash
export TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_API_KEY=your_api_key
```

Dry run:

```bash
./builddir/tizen-tool-domain-fetch youtube \
  --query 아이유 \
  --sp today \
  --count 6 \
  --dry-run \
  --format pretty
```

Live search:

```bash
./builddir/tizen-tool-domain-fetch youtube \
  --query 아이유 \
  --sp today \
  --count 6 \
  --format pretty
```

Compatibility notes:

- `--sp` stays as the external legacy input.
- Readable aliases are supported: `last-hour`, `today`, `week`, `month`, `year`
- For raw legacy tokens, configure one or more of:
  `TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_LAST_HOUR`,
  `TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_TODAY`,
  `TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_WEEK`,
  `TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_MONTH`,
  `TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_YEAR`
- Rolling semantics are used: `today=24h`, `week=7d`, `month=30d`, `year=365d`
- Time policy is fixed to `Asia/Seoul` (`KST`) before converting request bounds to UTC

### Fetch finance context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch finance --source mock --format pretty
```

Live KRW-first market payload:

```bash
./builddir/tizen-tool-domain-fetch finance \
  --source naver-public \
  --watchlist '005930:삼성전자,000660:SK하이닉스,035420:NAVER' \
  --format pretty
```

### Fetch commute context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch commute --source mock --format pretty
```

Live OSRM payload:

```bash
./builddir/tizen-tool-domain-fetch commute \
  --source osrm \
  --origin '망포역' \
  --destination '서초구청' \
  --profile driving \
  --format pretty
```

### Fetch sports context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch sports --source mock --format pretty
```

Live league payload:

```bash
./builddir/tizen-tool-domain-fetch sports \
  --source thesportsdb \
  --league kleague1 \
  --format pretty
```

### Fetch schedule context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch schedule --source mock --format pretty
```

Live ICS payload:

```bash
./builddir/tizen-tool-domain-fetch schedule \
  --source ics-url \
  --ics-url https://holidays.hyunbin.page/basic.ics \
  --days 7 \
  --max-events 6 \
  --format pretty
```

### Fetch travel context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch travel --source mock --format pretty
```

Live airport payload:

```bash
./builddir/tizen-tool-domain-fetch travel \
  --source airport-kr \
  --flight-number KE913 \
  --terminal T2 \
  --date 20260315 \
  --format pretty
```

### Fetch emergency context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch emergency --source mock --format pretty
```

Live KMA payload:

```bash
./builddir/tizen-tool-domain-fetch emergency \
  --source kma-combined \
  --format pretty
```

### Fetch daily context

Mock payload:

```bash
./builddir/tizen-tool-domain-fetch daily --source mock --format pretty
```

Live composed payload:

```bash
./builddir/tizen-tool-domain-fetch daily \
  --source compose-live \
  --format pretty
```

### Fetch additional mock-ready scenario context

These commands mirror the remaining TV scenario skills so downstream agents can
always ask `tizen-tool-domain-fetch` for a normalized payload, even when a live adapter is not
implemented yet.

```bash
./builddir/tizen-tool-domain-fetch family --source mock --format pretty
./builddir/tizen-tool-domain-fetch meal-delivery --source mock --format pretty
./builddir/tizen-tool-domain-fetch media --source mock --format pretty
./builddir/tizen-tool-domain-fetch shopping --source mock --format pretty
./builddir/tizen-tool-domain-fetch smart-home --source mock --format pretty
./builddir/tizen-tool-domain-fetch wellness --source mock --format pretty
```

Dry run works the same way for these scenario commands:

```bash
./builddir/tizen-tool-domain-fetch emergency --dry-run --format pretty
./builddir/tizen-tool-domain-fetch daily --source compose-live --dry-run --format pretty
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
