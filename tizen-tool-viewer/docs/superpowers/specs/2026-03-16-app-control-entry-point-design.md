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
│  ├─ OnCreate()             → 초기 인자를 _pendingPath에 저장  │
│  ├─ OnAppControlReceived() → 실행 중 재수신              │
│  └─ HandleAppControl()     → "file" 키 추출             │
│                                                        │
│  Flutter engine ready 후 MethodChannel로 전달            │
│                                                        │
└────────────────────┬───────────────────────────────────┘
                     │ MethodChannel("openclaw/a2ui")
                     ▼
┌─ Flutter Layer (Dart) ────────────────────────────────┐
│                                                        │
│  AppControlHandler                                     │
│  └─ MethodCallHandler → Stream<String> onFileReceived  │
│                                                        │
│  FilePayloadSource (A2uiPayloadSource 독립 구현)        │
│  └─ File(path).readAsString() → parseNdjson()          │
│                                                        │
│  HomeScreen                                            │
│  └─ Stream 구독 → 외부 파일 모드로 전환, 전체화면 렌더링    │
│                                                        │
│  GenUiScenarioSurface                                  │
│  └─ filePath 모드: NDJSON에서 surfaceId 자동 추출        │
│  └─ SurfaceController → Flutter 위젯 렌더링             │
│                                                        │
└────────────────────────────────────────────────────────┘
```

## Components

### 1. Tizen .NET Layer — Runner.cs

FlutterApplication을 상속하는 C# 클래스. AppControl 수신과 MethodChannel 전달만 담당하는 얇은 브릿지.

**Cold-start 타이밍 문제 해결:** `OnCreate()` 시점에는 Flutter 엔진이 아직 준비되지 않았을 수 있다. 초기 파일 경로를 `_pendingPath`에 저장하고, Flutter 측에서 `ready` 호출이 오면 전달한다.

```csharp
public class Runner : FlutterApplication
{
    const string ChannelName = "openclaw/a2ui";
    private MethodChannel _channel;
    private string _pendingPath;
    private bool _flutterReady = false;

    protected override void OnCreate()
    {
        base.OnCreate();
        _channel = new MethodChannel(ChannelName);

        // Flutter → C#: "ready" 신호 수신
        _channel.SetMethodCallHandler((method, args) =>
        {
            if (method == "ready")
            {
                _flutterReady = true;
                if (_pendingPath != null)
                {
                    _channel.InvokeMethod("loadFile", _pendingPath);
                    _pendingPath = null;
                }
            }
        });

        // 최초 실행 시 AppControl 인자 저장 (Flutter 아직 미준비)
        var filePath = ReceivedAppControl?.ExtraData?.TryGet("file");
        if (!string.IsNullOrEmpty(filePath))
        {
            _pendingPath = filePath;
        }
    }

    protected override void OnAppControlReceived(AppControlReceivedEventArgs e)
    {
        base.OnAppControlReceived(e);
        var filePath = e.ReceivedAppControl?.ExtraData?.TryGet("file");
        if (string.IsNullOrEmpty(filePath)) return;

        if (_flutterReady)
        {
            _channel.InvokeMethod("loadFile", filePath);
        }
        else
        {
            _pendingPath = filePath;  // Flutter 준비 후 전달
        }
    }
}
```

> **Note:** 실제 Tizen .NET Flutter 임베딩(`Tizen.Flutter.Embedding`) API는 구현 시점에 확인 필요. `MethodChannel` 생성 및 `SetMethodCallHandler` 패턴이 다를 수 있으며, `FlutterEngine.SendPlatformMessage()` 등의 API를 사용해야 할 수 있다.

**AppControl 키:**
- `file` — A2UI NDJSON 파일의 절대 경로

**CLI 호출:**
```bash
# 앱 실행 + 파일 전달
app_launcher -s org.openclaw.tv.genui -e file=/opt/openclaw/a2ui/weather.jsonl

# 실행 중인 앱에 새 파일 전달 (같은 명령)
app_launcher -s org.openclaw.tv.genui -e file=/opt/openclaw/a2ui/news.jsonl
```

**tizen-manifest.xml:**
```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns="http://tizen.org/ns/packages"
          api-version="6.5"
          package="org.openclaw.tv.genui"
          version="1.0.0">
  <profile name="tv" />
  <ui-application appid="org.openclaw.tv.genui"
                  exec="Runner.dll"
                  type="dotnet"
                  multiple="false"
                  taskmanage="true"
                  launch_mode="single">
    <label>OpenClaw TV</label>
    <app-control>
      <operation name="http://tizen.org/appcontrol/operation/default" />
    </app-control>
  </ui-application>
  <privileges>
    <privilege>http://tizen.org/privilege/mediastorage</privilege>
    <privilege>http://tizen.org/privilege/externalstorage</privilege>
  </privileges>
</manifest>
```

`launch_mode="single"` — 앱이 이미 실행 중이면 새 인스턴스를 만들지 않고 `OnAppControlReceived`로 전달.

### 2. AppControlHandler — MethodChannel 수신

새 파일: `lib/core/platform/app_control_handler.dart`

MethodChannel 리스너. C#에서 전달된 파일 경로를 broadcast Stream으로 변환. 초기화 시 C# 측에 `ready` 신호를 보내 pending path 전달을 트리거.

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

    // Flutter 엔진 준비 완료 신호 → C#의 pending path 전달 트리거
    _channel.invokeMethod('ready', null);
  }

  void dispose() => _filePathController.close();
}
```

> **Lifecycle note:** `AppControlHandler`는 `main()`에서 생성되며 앱 전체 수명 동안 유지된다. TV 앱은 정상적으로 종료되지 않으므로 `dispose()`가 호출되지 않는 것은 의도된 동작이다.

### 3. FilePayloadSource — 로컬 파일 읽기

새 파일: `lib/core/a2ui/file_payload_source.dart`

`A2uiPayloadSource`와 독립적인 구현. 기존 인터페이스의 `load(scenarioId)` 시그니처는 asset 기반 설계에 맞춰져 있으므로, 파일시스템 로딩은 별도 클래스로 구현하고 `GenUiScenarioSurface`에서 직접 사용한다.

```dart
class FilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    final raw = await File(filePath).readAsString();
    return parseNdjson(raw);
  }
}
```

`JsonFilePayloadSource`와의 차이:
- `rootBundle.loadString()` (asset) → `File().readAsString()` (파일시스템)
- `scenarioId` (논리적 이름) → `filePath` (절대 경로)
- `parseNdjson()`은 기존 유틸 그대로 재사용
- `A2uiPayloadSource`를 구현하지 않음 — 시맨틱 불일치 방지

### 4. GenUiScenarioSurface 변경 — 이중 모드 지원

수정 파일: `lib/features/home/widgets/genui_scenario_surface.dart`

기존 `scenario` (ScenarioEntry) 모드에 추가로 `filePath` 모드를 지원. 외부 파일 모드에서는 NDJSON의 `CreateSurface` 메시지에서 `surfaceId`를 자동 추출한다.

```dart
class GenUiScenarioSurface extends StatefulWidget {
  const GenUiScenarioSurface.scenario({
    required this.payloadSource,
    required this.scenario,
    super.key,
  }) : filePath = null;

  const GenUiScenarioSurface.file({
    required this.filePath,
    super.key,
  })  : payloadSource = null,
        scenario = null;

  final A2uiPayloadSource? payloadSource;
  final ScenarioEntry? scenario;
  final String? filePath;
}
```

**`_loadScenario()` 변경:**

```dart
Future<void> _loadScenario() async {
  final loadId = ++_loadId;
  _controller.dispose();
  _controller = _createController();

  try {
    final List<A2uiMessage> messages;

    if (widget.filePath != null) {
      // 외부 파일 모드: FilePayloadSource로 직접 로드
      messages = await FilePayloadSource().loadFile(widget.filePath!);
    } else {
      // 시나리오 카탈로그 모드: 기존 방식
      messages = await widget.payloadSource!.load(widget.scenario!.id);
    }

    if (!mounted || loadId != _loadId) return;

    // surfaceId를 CreateSurface 메시지에서 자동 추출
    final createMsg = messages.whereType<CreateSurface>().firstOrNull;
    final surfaceId = createMsg?.surfaceId
        ?? widget.scenario?.surfaceId
        ?? 'main';

    final newStyle = resolveSurfaceStyle(surfaceId);

    setState(() {
      _surfaceId = surfaceId;  // 새 필드: 동적 surfaceId
      _style = newStyle;
      _hasError = false;
    });

    for (final message in messages) {
      _controller.handleMessage(message);
    }
  } catch (_) {
    if (!mounted || loadId != _loadId) return;
    setState(() => _hasError = true);
  }
}
```

**`didUpdateWidget()` 변경 — null-safe 처리:**

```dart
@override
void didUpdateWidget(covariant GenUiScenarioSurface oldWidget) {
  super.didUpdateWidget(oldWidget);
  if (oldWidget.filePath != widget.filePath ||
      oldWidget.scenario?.id != widget.scenario?.id) {
    _loadScenario();
  }
}
```

기존 `oldWidget.scenario.id != widget.scenario.id`는 file 모드에서 null 크래시. `?.` 연산자로 변경.

**`build()` 변경 — `_surfaceId` 상태 필드 사용:**

```dart
String _surfaceId = 'main';  // 초기값, _loadScenario()에서 동적 갱신

@override
Widget build(BuildContext context) {
  // ...
  Surface(surfaceContext: _controller.contextFor(_surfaceId))
  // 기존 widget.scenario.surfaceId 대신 _surfaceId 사용
}
```

기존 `widget.scenario.surfaceId`는 file 모드에서 null 크래시. `_loadScenario()`에서 설정하는 `_surfaceId` 상태 필드로 교체.

**핵심:** 외부 파일 모드에서는 `ScenarioEntry`가 불필요. `surfaceId`와 `SurfaceStyle`을 NDJSON 내용에서 런타임에 추출한다.

### 5. HomeScreen 변경 — 외부 파일 모드 전환

수정 파일: `lib/features/home/home_screen.dart`

AppControlHandler의 Stream을 구독. 외부 파일이 오면 시나리오 목록을 숨기고 전체화면으로 렌더링.

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
  Widget build(BuildContext context) {
    // 외부 파일 모드: 전체화면 렌더링, 시나리오 목록 숨김
    if (_externalFilePath != null) {
      return GenUiScenarioSurface.file(
        key: ValueKey(_externalFilePath),
        filePath: _externalFilePath!,
      );
    }

    // 기존 모드: 시나리오 목록 + 프리뷰 패널
    return LayoutBuilder(
      builder: (context, constraints) {
        // ... 기존 _ScenarioRail + _PreviewPanel 레이아웃
      },
    );
  }

  @override
  void dispose() {
    _appControlSub?.cancel();
    super.dispose();
  }
}
```

외부 파일 모드에서는:
- `_ScenarioRail` 숨김 (시나리오 선택 불필요)
- `GenUiScenarioSurface.file()`로 전체화면 렌더링
- `ValueKey(_externalFilePath)`로 파일 변경 시 위젯 재생성
- 카탈로그 모드로 돌아가는 메커니즘은 현재 스코프 밖 (TV는 외부 제어 전용)

### 6. main.dart 및 OpenclawTvApp 변경

```dart
// main.dart
void main() {
  final appControlHandler = AppControlHandler();

  runApp(OpenclawTvApp(
    appControlHandler: appControlHandler,
  ));
}
```

```dart
// openclaw_tv_app.dart
class OpenclawTvApp extends StatelessWidget {
  const OpenclawTvApp({
    super.key,
    this.payloadSource,
    this.appControlHandler,
  });

  final A2uiPayloadSource? payloadSource;
  final AppControlHandler? appControlHandler;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      // ... 기존 설정
      home: HomeScreen(
        payloadSource: payloadSource ?? const JsonFilePayloadSource(),
        appControlHandler: appControlHandler,
      ),
    );
  }
}
```

`payloadSource`는 시나리오 카탈로그 모드에서만 사용. `appControlHandler`는 외부 파일 모드에서만 사용. 둘은 독립적이며, `appControlHandler`가 없으면 기존 동작 유지.

## Data Flow

```
app_launcher -e file=/path/to.jsonl
  ↓ Tizen AppControl
Runner: _pendingPath에 저장 (Flutter 미준비 시)
  ↓ Flutter "ready" 신호 수신 후
Runner: MethodChannel("openclaw/a2ui").InvokeMethod("loadFile", path)
  ↓ MethodChannel
AppControlHandler._filePathController.add(path)
  ↓ Stream<String>
HomeScreen setState(_externalFilePath = path)
  ↓ rebuild (전체화면 모드)
GenUiScenarioSurface.file(filePath: path)
  ↓ FilePayloadSource().loadFile(path)
  ↓ File(path).readAsString()
  ↓ parseNdjson() → List<A2uiMessage>
  ↓ CreateSurface에서 surfaceId 자동 추출
  ↓ SurfaceController.handleMessage() × 3 (create, data, components)
  ↓
Flutter 위젯 즉시 렌더링
```

## Edge Cases

**빠른 연속 호출 (race condition):**
두 번의 `app_launcher` 호출이 빠르게 연속되면 Stream이 두 경로를 emit. `GenUiScenarioSurface`의 기존 `_loadId` 가드가 stale async load를 방지하고, `ValueKey`로 위젯이 완전히 재생성되므로 안전.

**파일 없음 / 읽기 실패:**
`File(path).readAsString()`가 `FileSystemException` 발생 → 기존 `_loadScenario()`의 `catch (_)`에서 `_hasError = true` 설정 → 한국어 에러 메시지 표시. 상세 에러 UI는 스코프 밖.

**경로 검증:**
파일 경로는 Tizen `app_control`을 통해 전달되며, 호출자는 항상 신뢰할 수 있는 시스템 프로세스(Openclaw skill)다. 별도의 path traversal 검증은 수행하지 않는다.

**파일 확장자:**
기존 asset 파일은 `.json`, CLI 예시는 `.jsonl`을 사용하나, `parseNdjson()`은 확장자와 무관하게 NDJSON 내용을 처리한다. 둘 다 허용.

## New Files

| 파일 | 역할 |
|------|------|
| `tizen/Runner.cs` | Tizen .NET FlutterApplication, AppControl → MethodChannel (pending path 포함) |
| `tizen/tizen-manifest.xml` | Tizen 앱 매니페스트, `launch_mode="single"`, 파일 접근 권한 |
| `lib/core/platform/app_control_handler.dart` | MethodChannel 리스너, `ready` 핸드셰이크, Stream<String> 제공 |
| `lib/core/a2ui/file_payload_source.dart` | 로컬 파일시스템에서 NDJSON 로드 |

## Modified Files

| 파일 | 변경 내용 |
|------|-----------|
| `lib/main.dart` | AppControlHandler 생성 및 주입 |
| `lib/app/openclaw_tv_app.dart` | `appControlHandler` 파라미터 추가 |
| `lib/features/home/home_screen.dart` | Stream 구독, 외부 파일 모드 분기, 전체화면 렌더링 |
| `lib/features/home/widgets/genui_scenario_surface.dart` | `GenUiScenarioSurface.file()` 생성자, `_surfaceId` 상태 필드, `didUpdateWidget` null-safe, `build()`에서 `_surfaceId` 사용 |

## Not In Scope

- TV로의 파일 전송 (sdb push 등으로 별도 처리)
- 사용자 액션 피드백 (view 전용)
- HTTP 기반 payload source (Phase 2)
- 상세 에러 UI (파일 없음, 잘못된 JSON 등)
- 카탈로그 모드 복귀 메커니즘
