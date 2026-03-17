# Boss Demo Request Matrix

## Purpose

This document lists the kinds of natural-language requests a boss is likely to
try while standing in front of the TV and being told:

> "제가 TV에 AI agent 기능을 넣었습니다. 직접 시연해보세요."

The goal is not just to enumerate supported scenarios, but to anticipate the
questions that make the system feel like a real TV agent.

## How To Read This

- `Supported now`: Requests that map well to the current `tv_fetch` live
  adapters.
- `Stretch`: Requests that are adjacent to current capability and may work with
  good orchestration or prompt routing, but are not guaranteed.
- `Missing`: Requests a real boss is likely to try that are not covered by the
  current `tv_fetch` surface.

Current `tv_fetch` live coverage:

- `weather`
- `news`
- `news search`
- `finance`
- `commute`
- `sports`

Not yet covered in `tv_fetch` but already present as live scenario skills:

- `schedule`
- `travel`
- `emergency`

## Boss-Likely Requests

### 1. "Show me what matters right now"

These are high-risk demo requests because they sound natural and executive.

Supported now:

- `오늘 중요한 뉴스만 보여줘`
- `반도체 뉴스 검색해줘`
- `지금 나가면 서초구청까지 얼마나 걸려?`
- `8시까지 도착하려면 몇 시에 나가야 해?`
- `코스피랑 환율 한눈에 보여줘`
- `삼성전자랑 하이닉스 어때?`
- `오늘 스포츠 뭐 있어?`

Stretch:

- `지금 제일 중요한 것만 보여줘`
- `날씨 말고 시장 상황이 더 중요하면 그걸 먼저 보여줘`
- `뉴스 3개만 추려줘`

Missing:

- `오늘 중요한 거 다 한 번에 보여줘`
- `내 다음 일정이랑 출근길 같이 보여줘`
- `지금 특보 있으면 그것부터 보여줘`

What this implies:

- A real TV agent needs a `daily briefing` or `priority briefing` layer, not
  just domain fetchers.

### 2. "Make it personal"

A boss will often test whether the TV understands "my" context rather than
generic public data.

Supported now:

- `강남 날씨 보여줘`
- `망포역에서 서초구청까지 보여줘`
- `삼성전자 관련 뉴스 검색해줘`
- `내 watchlist만 보여줘`

Stretch:

- `서울 말고 판교 기준으로 보여줘`
- `운전 말고 걸어가면 얼마나 걸려?`
- `반도체 뉴스만 계속 보여줘`

Missing:

- `오늘 내 일정 보여줘`
- `다음 약속 뭐야?`
- `내 비행기 상태 보여줘`
- `우리 집 상태 보여줘`
- `내 배달 어디쯤 왔어?`

What this implies:

- Personal context requires `schedule`, `travel`, `smart home`, and `delivery`
  domains, plus a memory/config layer.

### 3. "Follow the conversation"

A boss will often probe whether the experience is conversational or just a
one-shot command launcher.

Supported now:

- `반도체 뉴스 검색해줘`
- `삼성전자 관련으로 바꿔줘`
- `3개만 보여줘`
- `운전 말고 도보로 바꿔줘`

Stretch:

- `아까 그 뉴스 다시 보여줘`
- `이거 더 간단하게`
- `이건 자세히 보여줘`
- `이거 큰 카드로 보여줘`

Missing:

- `두 번째 기사 자세히 보여줘`
- `그 일정 기준으로 출근길도 같이 보여줘`
- `아침마다 자동으로 이 화면 띄워줘`
- `이 형식으로 다음에도 기억해둬`

What this implies:

- The demo will feel much stronger if the system can handle follow-up edits,
  recall recent context, and switch layouts without starting over.

### 4. "Use the TV like a household device"

This is where bosses tend to test whether the system feels like a TV-native
agent instead of a dashboard.

Supported now:

- `지금 날씨 크게 띄워줘`
- `시장 상황 한눈에 보여줘`
- `K리그 결과 점수판처럼 보여줘`

Stretch:

- `지금 보는 중이라 오른쪽에만 띄워줘`
- `중요한 정보만 배너로 보여줘`

Missing:

- `현관문 잠겼어?`
- `거실 공기 괜찮아?`
- `이 드라마 출연진 보여줘`
- `지금 나오는 노래 뭐야?`
- `주문 상태 보여줘`

What this implies:

- `smart home`, `media companion`, and `meal delivery` are critical if the TV
  should feel like a real living-room agent.

## Boss Questions Most Likely To Expose Gaps

These are the questions most likely to make the current system feel incomplete.

1. `오늘 중요한 거 한 번에 보여줘`
2. `내 다음 일정이랑 출근길 같이 보여줘`
3. `지금 특보 있어?`
4. `KE913 상태 보여줘`
5. `오늘 일정 보여줘`
6. `아까 본 뉴스 다시 보여줘`
7. `이거 두 번째 기사 자세히 보여줘`
8. `현관문 잠겼는지 보여줘`
9. `배달 어디쯤 왔어?`
10. `지금 보는 사람 누구야?`

## Recommended Tool Priorities

If the goal is to maximize success against random boss demos, the next tool
investments should be:

1. `schedule`
Reason:
`오늘 일정 보여줘`, `다음 약속 뭐야?`, `몇 시 회의야?` are extremely likely.

2. `daily briefing`
Reason:
This answers the executive-style request: `오늘 중요한 거 다 보여줘`.

3. `emergency`
Reason:
`지금 특보 있어?` is short, natural, TV-appropriate, and high-impact.

4. `travel`
Reason:
`비행기 상태 보여줘`, `공항 정보 보여줘` feels concrete and premium in a demo.

5. `memory + follow-up orchestration`
Reason:
Without follow-ups, the system feels like a launcher, not an agent.

6. `smart home` or `media companion`
Reason:
These make the TV feel like an actual living-room assistant.

## Suggested Demo Pack

If we need a short scripted boss demo, these prompts provide a good arc:

1. `오늘 우산 챙겨야 해?`
2. `반도체 뉴스 검색해줘`
3. `지금 나가면 서초구청까지 얼마나 걸려?`
4. `코스피랑 환율 보여줘`
5. `K리그 결과 보여줘`
6. `오늘 중요한 거 다 한 번에 보여줘`

The first five show breadth. The sixth reveals whether the system behaves like
an orchestrating TV agent or just a collection of domain commands.

## Summary

The most important insight is this:

- A boss will not test `weather`, `news`, or `finance` as categories.
- A boss will test whether the TV understands `my morning`, `my context`, and
  `what matters now`.

That means the biggest perceived leap will come from:

- `schedule`
- `daily briefing`
- `emergency`
- `travel`
- memory and follow-up behavior
