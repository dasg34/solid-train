# tizen-tool-viewer

Samsung Tizen TV용 presentation-first viewer 앱. 전달받은 semantic
presentation JSON을 Flutter UI로 렌더링합니다.

## Architecture

```
Presentation JSON → A2uiPayloadSource → deterministic A2UI → SurfaceController → Surface → TV UI
```

- **A2uiPayloadSource**: presentation 자산이나 외부 payload를 A2UI 메시지로 변환해 주는 인터페이스
- **deterministic A2UI**: 앱 내부에서 기계적으로 생성되는 A2UI NDJSON
- **SurfaceController**: genui의 A2UI 메시지 처리 엔진
- **Surface**: genui의 Flutter 위젯 렌더러

Current flow:

```
Presentation JSON → PresentationAssetPayloadSource → deterministic A2UI → SurfaceController
```

- LLM은 로우레벨 `components` 트리 대신 의미 중심 JSON만 생성
- 앱이 카드, 차트, 알림, 요약 레이아웃을 기계적으로 조립
- 기본 시나리오 전체가 `assets/presentation/` 기반으로 동작

## Registered Components

TV 앱이 렌더링할 수 있는 11개 컴포넌트:

| Component | Description |
|-----------|-------------|
| Text | 텍스트 표시 (데이터 바인딩 지원) |
| Column | 세로 레이아웃 |
| Row | 가로 레이아웃 |
| Card | 카드 컨테이너 |
| Icon | Material Design 아이콘 |
| Divider | 구분선 |
| Button | 액션 버튼 |
| Inset | 단일 자식에 내부 패딩 적용 |
| Wrap | 여러 자식을 자동 줄바꿈 배치 |
| LineChart | 짧은 추세 시각화 |
| BarChart | 간단 비교 시각화 |

## Theme Shells

surfaceId 접두사에 따라 테마가 자동 적용됩니다:

- `weather_*` → 날씨 그라데이션 배경
- `news_*` → 뉴스 패널 배경
- `schedule_*` → 일정 패널 배경
- 그 외 → 기본 다크 테마

## Running

```bash
# Chrome
flutter run -d chrome

# Web server
flutter run -d web-server --web-hostname=127.0.0.1 --web-port=3000
```

## Presentation Assets

`assets/presentation/` 아래의 JSON이 앱의 기본 입력입니다.

- 입력: 제목, hero metric, secondary metrics, 시계열, facts, alert
- 출력: 동일한 데이터에서 deterministic A2UI NDJSON 생성
- 목적: LLM이 `Row/Column/Card`를 직접 배치하지 않도록 책임을 줄이기
- 미리보기: `flutter run --dart-define=OPENCLAW_DEFAULT_SCENARIO=finance_focus`

## File Structure

```
lib/
├── main.dart
├── app/openclaw_tv_app.dart           # 앱 진입점, A2uiPayloadSource 주입
├── core/
│   ├── a2ui/
│   │   ├── a2ui_payload_source.dart   # 추상 인터페이스
│   │   ├── parse_ndjson.dart          # 내부 deterministic A2UI 파서
│   │   └── surface_style.dart         # surfaceId → 테마 매핑
│   ├── presentation/
│   │   ├── presentation_surface.dart  # 의미 중심 입력 모델
│   │   ├── presentation_a2ui_builder.dart  # deterministic A2UI 변환기
│   │   ├── presentation_asset_payload_source.dart
│   │   ├── presentation_file_payload_source.dart
│   │   └── presentation_payload_decoder.dart
│   └── theme/app_theme.dart           # Material 3 다크 테마
└── features/home/
    ├── home_screen.dart               # 단일 풀스크린 surface 호스트
    ├── models/scenario_entry.dart     # 기본 fallback scenario 정의
    └── widgets/genui_scenario_surface.dart  # A2UI 렌더링 + 테마 쉘
assets/presentation/                   # built-in presentation scenarios
```

## Internal A2UI Format

앱은 presentation JSON을 내부적으로 아래 A2UI NDJSON 형태로 바꿉니다:

```
{"version":"v0.9","createSurface":{"surfaceId":"...","catalogId":"..."}}
{"version":"v0.9","updateDataModel":{"surfaceId":"...","value":{...}}}
{"version":"v0.9","updateComponents":{"surfaceId":"...","components":[...]}}
```

LLM-facing presentation JSON 규칙은
`/skills/tizen-tool-presentation-catalog/SKILL.md`를 참조하세요.

## Dependencies

- `genui` (local path: `../genui/packages/genui`) — A2UI 렌더링 엔진
- `google_fonts` — 한국어 폰트 (Noto Sans KR, Nanum Myeongjo)
- `flutter_localizations` — ko-KR 로케일
