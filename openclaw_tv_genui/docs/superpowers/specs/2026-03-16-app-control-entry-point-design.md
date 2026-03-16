# App Control Entry Point Design

TV 앱이 Tizen `app_control` CLI를 통해 외부 A2UI NDJSON 파일 경로를 수신하고 즉시 렌더링하는 진입점 설계.

## Context

현재 TV 앱은 번들된 asset 파일(`assets/a2ui/*.json`)에서만 A2UI 데이터를 읽는다. Openclaw skill이 생성한 A2UI NDJSON 파일을 TV에 전달하려면, 앱이 외부 파일 경로를 수신하고 렌더링할 수 있어야 한다.

### 요구사항

- Tizen `app_launcher` CLI로 파일 경로 전달
- 앱 최초 실행 시 인자 수신 + 이미 실행 중일 때 재수신 모두 지원
- 새 파일이 오면 현재 화면을 즉시 교체
- TV 로컬 파일시스템에 파일이 이미 존재한다고 가정
- 앱은 view 전용 (사용자 액션 피드백 없음)

### 제약사항

- Tizen .NET (C#) 플랫폼
- Flutter + genui 패키지 기반 렌더링
- A2UI v0.9 NDJSON 포맷

## Architecture

```
┌─ External (Openclaw Skill / CLI) ─────────────────────┐
│                                                        │
│  app_launcher -s org.openclaw.tv.genui                 │
│    -e file=/opt/openclaw/a2ui/weather.jsonl             │
│                                                        │
└────────────────────┬───────────────────────────────────┘
                     │ Tizen AppControl
                     ▼
┌─ Tizen .NET Layer (C#) ───────────────────────────────┐
│                                                        │
│  Runner.cs (FlutterApplication)                        │
│  ├─ OnCreate()             → 최초 실행 시 인자 추출     │
│  └─ OnAppControlReceived() → 실행 중 재수신             │
│  ├─ HandleAppControl()     → "file" 키 추출            │
│  └─ MethodChannel.InvokeMethod("loadFile", filePath)   │
│                                                        │
└────────────────────┬───────────────────────────────────┘
                     │ MethodChannel("openclaw/a2ui")
                     ▼
┌─ Flutter Layer (Dart) ────────────────────────────────┐
│                                                        │
│  AppControlHandler                                     │
│  └─ MethodCallHandler → Stream<String> onFileReceived  │
│                                                        │
│  FilePayloadSource (implements A2uiPayloadSource)      │
│  └─ File(path).readAsString() → parseNdjson()          │
│                                                        │
│  HomeScreen                                            │
│  └─ Stream 구독 → 새 파일이면 즉시 화면 교체             │
│                                                        │
│  GenUiScenarioSurface                                  │
│  └─ SurfaceController → Flutter 위젯 렌더링             │
│                                                        │
└────────────────────────────────────────────────────────┘
```

## Components

### 1. Tizen .NET Layer — Runner.cs

FlutterApplication을 상속하는 C# 클래스. AppControl 수신과 MethodChannel 전달만 담당하는 얇은 브릿지.

```csharp
public class Runner : FlutterApplication
{
    const string ChannelName = "openclaw/a2ui";

    protected override void OnCreate()
    {
        base.OnCreate();
        HandleAppControl(ReceivedAppControl);
    }

    protected override void OnAppControlReceived(AppControlReceivedEventArgs e)
    {
        base.OnAppControlReceived(e);
        HandleAppControl(e.ReceivedAppControl);
    }

    private void HandleAppControl(ReceivedAppControl appControl)
    {
        var filePath = appControl?.ExtraData?.TryGet("file");
        if (string.IsNullOrEmpty(filePath)) return;

        var channel = new MethodChannel(ChannelName);
        channel.InvokeMethod("loadFile", filePath);
    }
}
```

**AppControl 키:**
- `file` — A2UI NDJSON 파일의 절대 경로

**CLI 호출:**
```bash
# 앱 실행 + 파일 전달
app_launcher -s org.openclaw.tv.genui -e file=/opt/openclaw/a2ui/weather.jsonl

# 실행 중인 앱에 새 파일 전달 (같은 명령)
app_launcher -s org.openclaw.tv.genui -e file=/opt/openclaw/a2ui/news.jsonl
```

### 2. AppControlHandler — MethodChannel 수신

새 파일: `lib/core/platform/app_control_handler.dart`

MethodChannel 리스너. C#에서 전달된 파일 경로를 broadcast Stream으로 변환.

```dart
class AppControlHandler {
  static const _channel = MethodChannel('openclaw/a2ui');

  final _filePathController = StreamController<String>.broadcast();

  Stream<String> get onFileReceived => _filePathController.stream;

  AppControlHandler() {
    _channel.setMethodCallHandler((call) async {
      if (call.method == 'loadFile') {
        final path = call.arguments as String;
        _filePathController.add(path);
      }
    });
  }

  void dispose() => _filePathController.close();
}
```

### 3. FilePayloadSource — 로컬 파일 읽기

새 파일: `lib/core/a2ui/file_payload_source.dart`

기존 `A2uiPayloadSource` 인터페이스의 새 구현체. asset 대신 파일시스템에서 읽기.

```dart
class FilePayloadSource implements A2uiPayloadSource {
  @override
  Future<List<A2uiMessage>> load(String filePath) async {
    final raw = await File(filePath).readAsString();
    return parseNdjson(raw);
  }
}
```

`JsonFilePayloadSource`와의 차이:
- `rootBundle.loadString()` (asset) → `File().readAsString()` (파일시스템)
- `scenarioId` (이름) → `filePath` (절대 경로)
- `parseNdjson()`은 기존 유틸 그대로 재사용

### 4. HomeScreen 변경 — Stream 구독

수정 파일: `lib/features/home/home_screen.dart`

AppControlHandler의 Stream을 구독. 외부 파일이 오면 `_externalFilePath`를 설정하여 즉시 화면 교체.

```dart
class _HomeScreenState extends State<HomeScreen> {
  StreamSubscription<String>? _appControlSub;
  late ScenarioEntry _selectedScenario;
  String? _externalFilePath;

  @override
  void initState() {
    super.initState();
    _selectedScenario = scenarioCatalog.first;

    _appControlSub = widget.appControlHandler?.onFileReceived.listen((path) {
      setState(() {
        _externalFilePath = path;
      });
    });
  }

  @override
  void dispose() {
    _appControlSub?.cancel();
    super.dispose();
  }
}
```

렌더링 분기:
- `_externalFilePath != null` → `FilePayloadSource`로 외부 파일 렌더링
- `_externalFilePath == null` → 기존 시나리오 카탈로그 모드

### 5. main.dart 변경

```dart
void main() {
  final appControlHandler = AppControlHandler();

  runApp(OpenclawTvApp(
    appControlHandler: appControlHandler,
  ));
}
```

`OpenclawTvApp`에 `appControlHandler` 파라미터 추가하여 HomeScreen까지 전달.

## Data Flow

```
app_launcher -e file=/path/to.jsonl
  ↓ Tizen AppControl
Runner.HandleAppControl()
  ↓ MethodChannel("openclaw/a2ui", "loadFile", path)
AppControlHandler._filePathController.add(path)
  ↓ Stream<String>
HomeScreen setState(_externalFilePath = path)
  ↓ rebuild
GenUiScenarioSurface
  ↓ FilePayloadSource.load(path)
  ↓ File(path).readAsString()
  ↓ parseNdjson() → List<A2uiMessage>
  ↓ SurfaceController.handleMessage() × 3 (create, data, components)
  ↓
Flutter 위젯 즉시 렌더링
```

## New Files

| 파일 | 역할 |
|------|------|
| `tizen/Runner.cs` | Tizen .NET FlutterApplication, AppControl → MethodChannel |
| `tizen/tizen-manifest.xml` | Tizen 앱 매니페스트 (app_control 수신 설정) |
| `lib/core/platform/app_control_handler.dart` | MethodChannel 리스너, Stream<String> 제공 |
| `lib/core/a2ui/file_payload_source.dart` | 로컬 파일시스템 A2uiPayloadSource 구현체 |

## Modified Files

| 파일 | 변경 내용 |
|------|-----------|
| `lib/main.dart` | AppControlHandler 생성 및 주입 |
| `lib/app/openclaw_tv_app.dart` | appControlHandler 파라미터 추가 |
| `lib/features/home/home_screen.dart` | Stream 구독, 외부 파일 모드 분기 |

## Not In Scope

- TV로의 파일 전송 (sdb push 등으로 별도 처리)
- 사용자 액션 피드백 (view 전용)
- HTTP 기반 payload source (Phase 2)
- 에러 UI (파일 없음, 잘못된 JSON 등의 상세 에러 처리)
