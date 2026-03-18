# tv_a2ui_launcher

`tv_a2ui_launcher` is a small native CLI that reads A2UI NDJSON from `stdin`
or a file and sends a Tizen App Control launch request to
`com.example_tv_genui`.

Typical usage:

```bash
cat /tmp/a2ui.json | tv_a2ui_launcher
tv_a2ui_launcher --file /tmp/a2ui.json
tv_a2ui_launcher --file /tmp/a2ui.json --app-id com.example_tv_genui
tv_a2ui_launcher --file /tmp/a2ui.json --dry-run --format pretty
```

Behavior:

- `stdin` is the default input channel
- `--file` can be used instead of piping
- the launcher sends the raw NDJSON text using App Control extra data key
  `json`
- the target app ID defaults to `com.example_tv_genui`

On non-Tizen environments, the project builds with a stub launcher so local
development and tests can run without `app_control.h`.
