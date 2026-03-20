# 구현 소개 요약

## 무엇을 만들었나

이 프로젝트는 자연어 요청을 삼성 Tizen TV에서 보기 좋은 화면으로 바꿔 주는
viewer 시스템입니다.

예를 들면 이런 요청을 처리합니다.

- 날씨 보여줘
- 뉴스 보여줘
- 오늘 일정 보여줘
- 출근길 상황 보여줘

핵심 목표는 TV에서 읽기 쉬운 화면을 빠르게 보여주는 것입니다. 채팅 로그를
그대로 띄우는 것이 아니라, TV에 맞는 카드, 차트, 요약 정보 중심 화면으로
보여주는 방향으로 작업했습니다.

## 왜 구조를 바꿨나

초기에는 LLM이 TV UI를 위한 로우레벨 A2UI JSON을 직접 생성하도록 시도했습니다.
하지만 실제로 해보니 이 방식은 기대보다 안정적이지 않았습니다.

- 레이아웃 품질이 들쭉날쭉함
- 카드 배치와 간격이 자주 어색해짐
- 화면 구조가 필요 이상으로 복잡해짐
- 검증과 유지보수가 어려움

반면 LLM은 많은 양의 데이터를 보고, 그중에서 지금 보여줘야 할 핵심 정보만
고르는 일은 훨씬 잘했습니다.

이 점에 착안해서 구조를 다음처럼 바꿨습니다.

- LLM은 로우레벨 UI를 만들지 않음
- LLM은 데이터를 수집하고 정리된 presentation JSON만 만듦
- viewer 앱은 이 presentation JSON을 받아 기계적으로 UI로 변환함

즉, LLM은 "무엇을 보여줄지"를 결정하고, viewer는 "어떻게 보여줄지"를
결정하는 구조입니다.

## 전체 흐름

```text
사용자 요청
  -> 데이터 수집
  -> LLM이 필요한 정보만 추림
  -> presentation JSON 생성
  -> payload 검증
  -> viewer 앱으로 전달
  -> viewer가 deterministic A2UI로 변환
  -> TV 화면 렌더링
```

현재 파이프라인을 도구 기준으로 쓰면 다음과 같습니다.

```text
user request
  -> tizen-tool-domain-fetch
  -> tizen-tool-presentation-catalog
  -> tizen-tool-presentation-validate
  -> tizen-tool-viewer-launch
  -> tizen-tool-viewer
```

조금 더 직관적으로 표현하면 전체 흐름은 다음과 같습니다.

```text
[ 사용자 요청 ]
      |
      v
[ LLM / 에이전트 ]
  - 요청 의도 해석
  - 필요한 정보 종류 결정
      |
      v
[ 데이터 수집 / 외부 도구 ]
  - tizen-tool-domain-fetch
  - 기타 API / 스킬 / 외부 소스
      |
      v
[ LLM / 에이전트 ]
  - 수집된 데이터 정리
  - 중요한 정보만 선택
  - presentation JSON 생성
      |
      v
[ tizen-tool-presentation-validate ]
  - payload 형식 검증
      |
      v
[ tizen-tool-viewer-launch ]
  - viewer 앱으로 runtime handoff
      |
      v
[ tizen-tool-viewer ]
  - presentation JSON 수신
  - deterministic A2UI로 변환
  - TV UI 렌더링
      |
      v
[ TV 화면 ]
```

## 각 프로젝트 역할

### `tizen-tool-domain-fetch`

도메인 데이터를 가져오고 정규화하는 CLI입니다.

- weather
- news
- finance
- commute
- sports
- schedule
- travel
- emergency
- daily

여기서의 책임은 데이터 수집과 정규화까지입니다. UI는 만들지 않습니다.

### `tizen-tool-presentation-validate`

presentation JSON이 올바른 형식인지 확인하는 validator입니다.

- 필수 필드 확인
- 구조 검증
- 잘못된 payload 조기 차단

viewer로 보내기 전에 최소한의 안정성을 확보하는 역할입니다.

### `tizen-tool-viewer`

실제 TV 화면을 렌더링하는 Flutter 기반 viewer 앱입니다.

- presentation JSON 수신
- 내부에서 deterministic A2UI로 변환
- 카드, 차트, 요약 화면 렌더링

중요한 점은 viewer가 데이터를 직접 수집하지 않는다는 것입니다.
viewer는 presentation payload를 받아 렌더링만 담당합니다.

### `tizen-tool-viewer-launch`

presentation JSON을 runtime에 viewer 앱으로 전달하는 launcher입니다.

- `stdin` 또는 `--file` 입력 지원
- Tizen App Control로 viewer 실행
- cold start 상황에서도 payload 전달 보강

즉, viewer를 띄우고 payload를 넘겨주는 handoff 도구입니다.

## 스킬 역할

LLM이 이 구조를 안정적으로 쓰도록 몇 가지 스킬도 같이 정리했습니다.

### `tizen-tool-domain-fetch`

어떤 도메인 데이터를 어떤 방식으로 가져올지 안내하는 스킬입니다.

### `tizen-tool-presentation-catalog`

LLM이 presentation JSON을 어떤 형태로 만들어야 하는지 알려주는 스킬입니다.

여기서 중요한 점은 로우레벨 UI 트리를 직접 생성하는 것이 아니라,
의미 중심의 formatted data JSON을 만들게 한다는 점입니다.

예를 들면 이런 정보들입니다.

- title
- summary
- hero
- metrics
- facts
- chart
- alert

### `tizen-tool-viewer-launcher`

완성된 payload를 runtime handoff 흐름으로 넘길 때 쓰는 스킬입니다.

## 핵심 아이디어

이 작업의 핵심 아이디어는 아주 단순합니다.

LLM에게 TV용 로우레벨 GUI JSON을 직접 만들게 하면 결과가 불안정했습니다.
하지만 LLM은 데이터에서 중요한 것만 뽑고, 의미 있는 형태로 정리하는 일은
상대적으로 잘했습니다.

그래서 역할을 다음처럼 분리했습니다.

- LLM: 데이터 수집, 정보 선택, presentation JSON 생성
- validator: 형식 검증
- viewer: presentation JSON을 deterministic A2UI로 변환 후 렌더링

이 구조 덕분에 얻은 장점은 다음과 같습니다.

- 화면 품질이 훨씬 안정적임
- TV용 레이아웃을 일관되게 유지할 수 있음
- LLM이 레이아웃 실수를 덜 하게 됨
- payload 검증이 쉬워짐
- viewer 쪽 개선과 데이터 쪽 개선을 분리해서 진행할 수 있음

## 한 줄 요약

이 프로젝트는 LLM이 직접 TV UI를 그리게 하는 대신, 필요한 정보만 정리한
presentation JSON을 만들게 하고, viewer 앱이 그것을 기계적으로 UI로 변환해
안정적인 TV 화면을 보여주는 구조로 정리한 작업입니다.
