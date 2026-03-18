---
name: tv-surface-launcher
description: End-to-end TV surface orchestration for Korean natural-language display requests such as '날씨 보여줘', '뉴스 알려줘', '일정 보여줘', '출근길 보여줘', or '오늘 브리핑 띄워줘'. Use when Codex should map the request to a supported tv_fetch domain, ask one short follow-up only if a required fetch input is missing, generate valid TV A2UI NDJSON with the tv-a2ui-catalog rules, save it to a local file, validate it, and launch it through tv_a2ui_launcher.
---

# TV Surface Launcher

Turn a short user utterance into a launched TV surface. Use this skill when
the user wants an end-to-end result, not just a fetched JSON blob or a draft
of A2UI.

## Workflow

1. Infer the scenario from the noun phrase in the request, not from verbs like
   `보여줘`, `알려줘`, `띄워줘`, or `브리핑해줘`.
2. If the scenario is unclear or a required fetch input is missing, ask one
   short Korean follow-up question and wait. Ask only for the single missing
   piece.
3. Inspect the available fetch contract before calling the fetcher:

   ```bash
   tv_fetch describe --format pretty
   tv_fetch describe <domain> --format pretty
   ```

4. Use `tv-fetch` if you need its CLI etiquette or missing-input reminders,
   then fetch normalized domain JSON from `tv_fetch`.
5. Use `tv-a2ui-catalog` to generate valid A2UI v0.9 NDJSON.
6. Save the generated NDJSON directly to a local file. Do not keep it only in
   the chat response.
7. Validate the saved file with
   `tv_a2ui_validate`.
8. Launch the exact saved file with
   `cat <file-name> | tv_a2ui_launcher`.
9. Report the selected domain, source, saved file path, and launch result.

Assume these CLIs are already callable directly:

```bash
tv_fetch
tv_a2ui_validate
tv_a2ui_launcher
```

Do not save generated NDJSON into the git repo unless the user explicitly asks.
Default to `/tmp/tv-a2ui/`.

## Domain Routing

Map common Korean requests like this:

- `날씨`, `기온`, `비`, `강수`, `체감온도` -> `weather`
- `뉴스`, `헤드라인`, `속보` -> `news`
- `일정`, `캘린더`, `회의`, `행사` -> `schedule`
- `출근`, `퇴근`, `언제 나가`, `길찾기`, `통근` -> `commute`
- `주식`, `환율`, `시황`, `증시`, `금융` -> `finance`
- `경기`, `점수`, `순위`, `스포츠` -> `sports`
- `공항`, `비행기`, `탑승`, `게이트`, `출국` -> `travel`
- `재난`, `경보`, `지진`, `특보` -> `emergency`
- `브리핑`, `모닝 브리핑`, `오늘 요약`, `데일리` -> `daily`
- `가족 게시판` -> `family`
- `배달`, `주문 상태`, `도착 예정` -> `meal-delivery`
- `미디어`, `에피소드`, `출연진`, `사운드트랙` -> `media`
- `쇼핑`, `비교`, `추천 비교` -> `shopping`
- `스마트홈`, `집 상태`, `문 잠김`, `실내 온도` -> `smart-home`
- `웰니스`, `운동`, `수면`, `스트레칭` -> `wellness`

If no supported domain is clear, ask a short clarification question instead of
guessing.

## Fetch Step

Do not duplicate the fetch contract here. Follow `tv-fetch` for
domain-specific parameters, live-vs-mock source choices, and missing-input
questions.

Keep only these launcher-specific fetch reminders:

- Run `tv_fetch describe` first when the current domain contract is not already
  clear in this turn.
- Fetch the actual payload with `--format json`.
- Ask at most one short follow-up question when a required fetch input is
  missing.
- Do not invent personal inputs such as location, route endpoints, watchlists,
  calendar sources, or flight details.

## A2UI Step

Do not restate the catalog rules here. Follow `tv-a2ui-catalog` for component
constraints, message structure, TV layout rules, and examples, then return to
this skill for file save, validation, and launch.

## Save, Validate, Launch

Write the final NDJSON to a concrete file path such as
`/tmp/tv-a2ui/weather-20260318-161500.ndjson`. Use plain file output, not a
helper script. A simple pattern is:

```bash
mkdir -p /tmp/tv-a2ui
cat > /tmp/tv-a2ui/weather-20260318-161500.ndjson <<'EOF'
{"version":"v0.9","createSurface":{"surfaceId":"weather-20260318-161500","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{"domain":"weather","pattern":"centerCard"}}}
{"version":"v0.9","updateDataModel":{"surfaceId":"weather-20260318-161500","value":{"title":"서울 날씨","temp":"18°","condition":"맑음"}}}
{"version":"v0.9","updateComponents":{"surfaceId":"weather-20260318-161500","components":[{"id":"root","component":"Column","children":["title"]},{"id":"title","component":"Text","text":{"path":"/title"},"variant":"h3"}]}}
EOF
```

Then validate and launch that exact file:

```bash
tv_a2ui_validate /tmp/tv-a2ui/weather-20260318-161500.ndjson
cat /tmp/tv-a2ui/weather-20260318-161500.ndjson | tv_a2ui_launcher
```

For local verification without sending a real launch request, add
`--dry-run --format pretty` to the launcher command.

## Response Contract

After finishing the workflow, briefly tell the user:

- which domain and source were used
- which follow-up assumptions were made, if any
- where the NDJSON file was saved
- whether validation passed
- whether the launcher was executed successfully
