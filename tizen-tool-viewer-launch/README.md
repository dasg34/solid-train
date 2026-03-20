# tizen-tool-viewer-launch

`tizen-tool-viewer-launch` is a small native CLI that reads presentation JSON from
`stdin` or a file and sends a Tizen App Control launch request to
`org.tizen.tizen-tool-viewer`.

Typical usage:

```bash
cat /tmp/presentation.json | tizen-tool-viewer-launch
tizen-tool-viewer-launch --file /tmp/presentation.json
tizen-tool-viewer-launch --file /tmp/presentation.json --app-id org.tizen.tizen-tool-viewer
tizen-tool-viewer-launch --file /tmp/presentation.json --dry-run --format pretty
```

Behavior:

- `stdin` is the default input channel
- `--file` can be used instead of piping
- the launcher sends the raw presentation JSON text using App Control extra
  data key `json`
- the target app ID defaults to `org.tizen.tizen-tool-viewer`
- on Tizen, the launcher replays the same launch request once after a short
  delay to improve cold-start delivery when the Flutter app is still booting

On non-Tizen environments, the project builds with a stub launcher so local
development and tests can run without `app_control.h`.
