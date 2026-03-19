---
name: tizen-tool-presentation-catalog
description: >
  Use this skill when generating presentation JSON for the Samsung Tizen TV
  app. The LLM should output one semantic TV presentation JSON object. The app
  handles the final rendering internally.
---

# Tizen Tool Presentation Catalog

This skill defines the preferred LLM output for the OpenClaw TV app.

The LLM outputs one presentation JSON object only. The app handles the final
UI assembly internally.

## Output Rule

Emit one raw JSON object only.

- Do NOT output NDJSON.
- Do NOT output protocol envelopes, component updates, or low-level layout trees.
- Do NOT wrap the payload in Markdown fences.
- Do NOT add prose before or after the JSON.
- The first character must be `{`.

## Validation Rule

If the generated JSON will be saved, previewed, or handed off to the TV app,
validate it with `tizen-tool-presentation-validate` before delivery.

Preferred command:

```bash
tizen-tool-presentation-validate /tmp/tv-presentation/<domain>-<timestamp>.json --format pretty
```

If the payload has not been saved to a file yet, stdin validation is also
allowed:

```bash
cat /tmp/tv-presentation/<domain>-<timestamp>.json | \
  tizen-tool-presentation-validate --stdin
```

Validation is not part of the JSON payload itself. Do not mention validator
commands inside the output JSON.

## Missing Information Rule

Do not invent required rendering inputs.

- If `theme.domain` is unclear, ask one short question.
- If the request needs a user-specific value such as location, watchlist, or
  destination and upstream data did not provide it, ask one short question.
- If the domain is clear but the exact visual emphasis is vague, choose a
  reasonable `theme.pattern`.

## Presentation Model

Generate one JSON object with this shape.

### Required fields

- `surfaceId`: unique string such as `weather_today` or `finance_focus`
- `theme.domain`: domain such as `weather`, `news`, `finance`, `schedule`
- `theme.pattern`: one of `immersive`, `sidePanel`, `centerCard`,
  `topBanner`, `bottomRibbon`
- `title`: primary heading
- `hero.label`: label for the main metric
- `hero.value`: main metric value

### Optional fields

- `theme.scale`: `compact`, `standard`, or `expanded`
- `summary`: one short sentence under the title
- `hero.detail`: supporting metric such as feels-like temperature or day change
- `hero.caption`: timestamp or trust label
- `metrics`: array of 1-4 `{label, value, detail?}` objects
- `facts`: array of 1-4 `{label, value, detail?}` objects
- `chart`: chart object
- `alert`: alert object

### Chart

```json
{
  "title": "오전 기온 추이",
  "kind": "line",
  "labels": ["07시", "08시", "09시", "10시"],
  "values": [-2.0, -0.9, 0.8, 2.7],
  "unitLabel": "기온(°C)",
  "detail": "6시간 snapshot 기준"
}
```

Rules:

- `kind` must be `line` or `bar`
- `values` must be a numeric array with at least 2 points
- if `labels` is present, it must match `values` length
- prefer 4-8 points for TV readability

### Alert

```json
{
  "title": "공식 특보 연동 필요",
  "summary": "재난성 특보는 기상청 공식 채널과 별도로 연동하세요.",
  "meta": "Open-Meteo · 2026-03-19 07:45"
}
```

## Field Selection Rules

Add optional fields only when the source data or user request requires them.

- `summary`: include only when there is a useful one-line description
- `metrics`: include when there are supporting `{label, value}` pairs
- `facts`: include when there are additional short fact pairs
- `chart`: include when ordered numeric data exists
- `alert`: include when there is a warning, caveat, or operational notice

Do not force optional fields to appear.

- Sparse requests such as "삼성전자만 보여줘" can be just `title + hero`
- Richer requests can add `metrics`, `chart`, `facts`, and `alert`

## What Not To Output

- Protocol envelopes or NDJSON
- Low-level component trees
- Footer text blocks
- Hidden implementation details such as icon names or chart colors

## Example

```json
{
  "surfaceId": "weather_today",
  "theme": {
    "domain": "weather",
    "pattern": "immersive"
  },
  "title": "서울 서초구",
  "summary": "서울 현재 맑음, 체감 -5°입니다.",
  "hero": {
    "label": "현재 기온",
    "value": "-1°",
    "detail": "체감 -5°",
    "caption": "2026-03-19 07:45 기준"
  },
  "metrics": [
    {"label": "상태", "value": "맑음"},
    {"label": "습도", "value": "56%"},
    {"label": "강수확률", "value": "0%"}
  ],
  "chart": {
    "title": "오전 기온 추이",
    "kind": "line",
    "labels": ["07시", "08시", "09시", "10시"],
    "values": [-2.0, -0.9, 0.8, 2.7],
    "unitLabel": "기온(°C)"
  },
  "alert": {
    "title": "공식 특보 연동 필요",
    "summary": "재난성 특보는 기상청 공식 채널과 별도로 연동하세요.",
    "meta": "Open-Meteo · 2026-03-19 07:45"
  }
}
```
