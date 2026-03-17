# tv_fetch

Agent-friendly C++ CLI for fetching and normalizing TV domain context.

`tv_fetch` is the data-only layer for the OpenClaw TV pipeline:

```text
user request -> domain fetch -> normalized JSON -> A2UI composition -> viewer app
```

This project intentionally stops at **normalized domain JSON**. It does not
generate A2UI and it does not launch the viewer.

For Tizen packaging, the repository includes the RPM spec at
[packaging/tv_fetch.spec](/Users/yohoho/work/tv_fetch/packaging/tv_fetch.spec).

## Design goals

- JSON-first output for humans and AI agents
- Self-describing commands via `describe`
- Deterministic normalized payloads for downstream A2UI composition
- Input hardening for agent-passed arguments
- Small dependency surface: `libcurl` + vendored `nlohmann/json`

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

## Output shape

The weather command emits normalized JSON matching the weather skill input
contract closely:

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

## Roadmap

- Add more fetchers: news, schedule, commute, finance
- Add top-level `schema` command for output contracts
- Add stable machine-readable error codes across all domains
