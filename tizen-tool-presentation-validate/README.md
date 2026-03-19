# tizen-tool-presentation-validate

`tizen-tool-presentation-validate` is a C++ validator for the Samsung Tizen TV
presentation JSON payload used by `openclaw_tv_genui`.

It checks the semantic presentation object before the Flutter app converts it
into deterministic A2UI.

## What it validates

- Raw JSON object only
- Required fields: `surfaceId`, `theme`, `title`, `hero`
- Theme shape and allowed `pattern`/`scale` values
- `metrics` and `facts` item shape
- `chart` shape, numeric values, and label/value length match
- `alert` shape
- Forbidden fields like `footer`
- Rejection of legacy raw A2UI envelope payloads
- TV density warnings for overly busy payloads

## Usage

Validate a file:

```bash
tizen-tool-presentation-validate /path/to/payload.json --format pretty
```

Read from stdin:

```bash
cat /path/to/payload.json | tizen-tool-presentation-validate --stdin
```

Show built-in validation rules:

```bash
tizen-tool-presentation-validate describe --format pretty
```

## Tizen packaging

Packaging files are in `packaging/`.

- `packaging/tizen-tool-presentation-validate.spec`
- `packaging/tizen-tool-presentation-validate.manifest`

