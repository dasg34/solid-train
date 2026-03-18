---
name: tv-fetch
description: "TV 도메인 데이터를 가져올 때 사용하는 tv_fetch CLI 가이드. 날씨, 뉴스, 금융, 출퇴근, 스포츠 등 TV 시나리오 데이터가 필요하거나, tv_fetch 실행·확장할 때 이 스킬을 사용한다. '데이터 가져와', 'mock 데이터', 'fetch' 같은 요청에도 트리거한다. 필수 입력이 빠지면 임의로 추정하지 말고 사용자에게 짧게 다시 묻는다."
---

# tv_fetch CLI

OpenClaw TV 파이프라인의 데이터 레이어. 정규화된 도메인 JSON을 생산하며, presentation JSON 생성이나 뷰어 실행은 하지 않는다.

```
user request → tv_fetch (normalized JSON) → presentation composition → viewer app
```

## 사용법

tv_fetch는 self-describing CLI다. 도메인 목록, 옵션, source 종류는 CLI에 직접 물어본다:

```bash
tv_fetch describe --format pretty           # 전체 CLI 구조
tv_fetch describe <domain> --format pretty  # 특정 도메인 옵션
```

처음 사용할 때는 반드시 `describe`부터 호출해서 사용 가능한 도메인과 옵션을 파악한다.

## 핵심 패턴

1. **`describe` 먼저** — 도메인별 옵션이 기억나지 않으면 CLI에 물어본다.
2. **`--format pretty`** — 사람이 읽을 때. 파이프라인 연결 시 생략하면 JSON 기본.
3. **`--dry-run`** — live source에서 실제 HTTP 요청 없이 구성만 확인.

## 필수 정보가 없을 때

- 필수 입력이 비어 있으면 임의 기본값을 넣지 말고 사용자에게 다시 묻는다.
- 질문은 한 번에 하나의 짧은 확인 질문으로 제한한다.
- 사용자가 범위를 느슨하게 줬다면 그 안에서만 합리적으로 보정한다.

대표 예시:
- `weather`에서 위치가 없으면 지역을 다시 묻는다.
- `commute`에서 `origin` 또는 `destination`이 없으면 빠진 쪽을 다시 묻는다.
- `finance`에서 특정 종목 요청인지 시장 전체 요청인지 불명확하면 관심 종목인지 시장 요약인지 다시 묻는다.
- `news`에서 검색 의도는 보이는데 키워드가 비어 있으면 검색어를 다시 묻는다.

## 예시

```bash
# 서울 날씨 (live)
tv_fetch weather --city 서울 --district 중구 --format pretty

# 최신 뉴스 헤드라인
tv_fetch news --format pretty

# 키워드 뉴스 검색
tv_fetch news --query 반도체 --count 6 --format pretty

# 금융 — 관심 종목 지정
tv_fetch finance --watchlist '005930:삼성전자,000660:SK하이닉스' --format pretty

# 출퇴근 경로
tv_fetch commute --origin '망포역' --destination '서초구청' --format pretty

# mock 데이터 (모든 도메인 공통)
tv_fetch weather --source mock --format pretty
```

## 시나리오 참고 문서

각 시나리오의 feasibility 분석과 live data 소스 문서:

```
/Users/yohoho/work/skills/tv-scenario-references/
├── scenario-feasibility-matrix.md
├── weather-briefing/feasibility.md, live-data.md
├── news-briefing/feasibility.md, live-data.md
└── ...
```

## 다음 단계: Presentation 변환

tv_fetch 출력을 TV 화면으로 만들려면 `tv-a2ui-catalog` 스킬을 참고하여 presentation JSON으로 변환한다.
