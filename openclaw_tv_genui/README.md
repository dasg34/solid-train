# OpenClaw TV GenUI

Samsung Tizen TV용 A2UI 렌더링 앱. OpenClaw에서 전달받은 A2UI JSON을 Flutter
UI로 렌더링합니다.

## Architecture

```
A2UI JSON (NDJSON) → A2uiPayloadSource → SurfaceController → Surface → TV UI
```

- **A2uiPayloadSource**: A2UI 메시지 로딩 인터페이스
  - Phase 1: `JsonFilePayloadSource` (로컬 JSON 파일)
  - Phase 2: HTTP 또는 스트리밍 프로토콜
- **SurfaceController**: genui의 A2UI 메시지 처리 엔진
- **Surface**: genui의 Flutter 위젯 렌더러

## Registered Components

TV 앱이 렌더링할 수 있는 7개 컴포넌트:

| Component | Description |
|-----------|-------------|
| Text | 텍스트 표시 (데이터 바인딩 지원) |
| Column | 세로 레이아웃 |
| Row | 가로 레이아웃 |
| Card | 카드 컨테이너 |
| Icon | Material Design 아이콘 |
| Divider | 구분선 |
| Button | 액션 버튼 |

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

## File Structure

```
lib/
├── main.dart
├── app/openclaw_tv_app.dart           # 앱 진입점, A2uiPayloadSource 주입
├── core/
│   ├── a2ui/
│   │   ├── a2ui_payload_source.dart   # 추상 인터페이스
│   │   ├── json_file_payload_source.dart  # Phase 1 구현
│   │   └── surface_style.dart         # surfaceId → 테마 매핑
│   └── theme/app_theme.dart           # Material 3 다크 테마
└── features/home/
    ├── home_screen.dart               # 단일 풀스크린 surface 호스트
    ├── models/scenario_entry.dart     # 기본 fallback scenario 정의
    └── widgets/genui_scenario_surface.dart  # A2UI 렌더링 + 테마 쉘
assets/a2ui/                           # 프리빌드된 A2UI NDJSON (15개)
```

## A2UI JSON Format

앱이 소비하는 NDJSON 형식 (줄당 하나의 JSON):

```
{"version":"v0.9","createSurface":{"surfaceId":"...","catalogId":"..."}}
{"version":"v0.9","updateDataModel":{"surfaceId":"...","value":{...}}}
{"version":"v0.9","updateComponents":{"surfaceId":"...","components":[...]}}
```

A2UI 생성 규칙은 `/skills/tv-a2ui-catalog/SKILL.md`를 참조하세요.

## Dependencies

- `genui` (local path: `../genui/packages/genui`) — A2UI 렌더링 엔진
- `google_fonts` — 한국어 폰트 (Noto Sans KR, Nanum Myeongjo)
- `flutter_localizations` — ko-KR 로케일
