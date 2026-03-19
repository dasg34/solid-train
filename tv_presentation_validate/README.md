# tv_presentation_validate

`tv_presentation_validate` is a C++ validator for the Samsung Tizen TV
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

## Build

```bash
cd /Users/yohoho/work/tv_presentation_validate
meson setup builddir
meson compile -C builddir
```

## Test

```bash
cd /Users/yohoho/work/tv_presentation_validate
meson test -C builddir
```

## Usage

Validate a file:

```bash
./builddir/tv_presentation_validate /path/to/payload.json --format pretty
```

Read from stdin:

```bash
cat /path/to/payload.json | ./builddir/tv_presentation_validate --stdin
```

Show built-in validation rules:

```bash
./builddir/tv_presentation_validate describe --format pretty
```

## Tizen packaging

Packaging files are in `packaging/`.

- `packaging/tv_presentation_validate.spec`
- `packaging/tv_presentation_validate.manifest`

The packaging layout mirrors `tv_a2ui_validate` so it can be built in the same
Tizen native CLI environment.
