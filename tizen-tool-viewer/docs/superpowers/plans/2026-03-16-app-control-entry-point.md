# App Control Entry Point Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable the TV app to receive external A2UI NDJSON file paths via Tizen `app_control` CLI and render them immediately.

**Architecture:** Thin C# Tizen bridge receives `app_control` events, extracts the `file` extra-data key, and sends the path to Flutter via MethodChannel. Flutter side uses a broadcast Stream to deliver file paths to the UI layer, which switches to full-screen rendering mode using a new `FilePayloadSource` that reads from the local filesystem.

**Tech Stack:** Dart/Flutter, C# (Tizen .NET), genui package, MethodChannel

**Spec:** `docs/superpowers/specs/2026-03-16-app-control-entry-point-design.md`

---

## Chunk 1: Flutter Data Layer (FilePayloadSource + parseNdjson extraction)

### Task 1: Extract `parseNdjson` to shared utility file

`parseNdjson()` currently lives in `json_file_payload_source.dart`. `FilePayloadSource` will also need it, but should not import from `json_file_payload_source.dart`. Extract to a shared file.

**Files:**
- Create: `lib/core/a2ui/parse_ndjson.dart`
- Modify: `lib/core/a2ui/json_file_payload_source.dart`
- Modify: `test/core/a2ui/json_file_payload_source_test.dart`

- [ ] **Step 1: Create `parse_ndjson.dart` with the extracted function**

```dart
// lib/core/a2ui/parse_ndjson.dart
import 'dart:convert';

import 'package:genui/genui.dart';

/// Parses an NDJSON string into a list of [A2uiMessage].
List<A2uiMessage> parseNdjson(String ndjson) {
  return ndjson
      .split('\n')
      .where((line) => line.trim().isNotEmpty)
      .map((line) =>
          A2uiMessage.fromJson(jsonDecode(line) as Map<String, Object?>))
      .toList();
}
```

- [ ] **Step 2: Update `json_file_payload_source.dart` to import from the new file**

Remove the `parseNdjson` function body and replace with an import + re-export:

```dart
// lib/core/a2ui/json_file_payload_source.dart
import 'package:flutter/services.dart' show rootBundle;
import 'package:genui/genui.dart';

import 'a2ui_payload_source.dart';
import 'parse_ndjson.dart';

export 'parse_ndjson.dart' show parseNdjson;

/// Loads A2UI messages from pre-generated NDJSON asset files.
class JsonFilePayloadSource implements A2uiPayloadSource {
  const JsonFilePayloadSource();

  @override
  Future<List<A2uiMessage>> load(String scenarioId) async {
    final raw = await rootBundle.loadString('assets/a2ui/$scenarioId.json');
    return parseNdjson(raw);
  }
}
```

- [ ] **Step 3: Run existing tests to verify nothing broke**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test test/core/a2ui/`
Expected: All tests pass. The test imports `parseNdjson` from `json_file_payload_source.dart` which now re-exports it.

- [ ] **Step 4: Commit**

```bash
git add lib/core/a2ui/parse_ndjson.dart lib/core/a2ui/json_file_payload_source.dart
git commit -m "refactor: extract parseNdjson to shared utility file"
```

### Task 2: Create FilePayloadSource

**Files:**
- Create: `lib/core/a2ui/file_payload_source.dart`
- Create: `test/core/a2ui/file_payload_source_test.dart`

- [ ] **Step 1: Write the failing test**

```dart
// test/core/a2ui/file_payload_source_test.dart
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/file_payload_source.dart';

void main() {
  group('FilePayloadSource', () {
    late Directory tempDir;

    setUp(() {
      tempDir = Directory.systemTemp.createTempSync('file_payload_source_test');
    });

    tearDown(() {
      tempDir.deleteSync(recursive: true);
    });

    test('loads NDJSON from filesystem path', () async {
      final file = File('${tempDir.path}/test.jsonl');
      file.writeAsStringSync([
        jsonEncode({
          'version': 'v0.9',
          'createSurface': {
            'surfaceId': 'test_main',
            'catalogId': 'test_catalog',
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateDataModel': {
            'surfaceId': 'test_main',
            'value': {'title': 'Hello'},
          },
        }),
        jsonEncode({
          'version': 'v0.9',
          'updateComponents': {
            'surfaceId': 'test_main',
            'components': [
              {'id': 'root', 'component': 'Column', 'children': []},
            ],
          },
        }),
      ].join('\n'));

      final source = FilePayloadSource();
      final messages = await source.loadFile(file.path);

      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });

    test('throws when file does not exist', () async {
      final source = FilePayloadSource();
      expect(
        () => source.loadFile('${tempDir.path}/nonexistent.jsonl'),
        throwsA(isA<FileSystemException>()),
      );
    });
  });
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test test/core/a2ui/file_payload_source_test.dart`
Expected: FAIL — `file_payload_source.dart` does not exist.

- [ ] **Step 3: Write minimal implementation**

```dart
// lib/core/a2ui/file_payload_source.dart
import 'dart:io';

import 'package:genui/genui.dart';

import 'parse_ndjson.dart';

/// Loads A2UI messages from a local filesystem NDJSON file.
class FilePayloadSource {
  Future<List<A2uiMessage>> loadFile(String filePath) async {
    final raw = await File(filePath).readAsString();
    return parseNdjson(raw);
  }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test test/core/a2ui/file_payload_source_test.dart`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add lib/core/a2ui/file_payload_source.dart test/core/a2ui/file_payload_source_test.dart
git commit -m "feat: add FilePayloadSource for local filesystem NDJSON loading"
```

## Chunk 2: Platform Bridge (AppControlHandler)

### Task 3: Create AppControlHandler

**Files:**
- Create: `lib/core/platform/app_control_handler.dart`
- Create: `test/core/platform/app_control_handler_test.dart`

- [ ] **Step 1: Write the failing test**

```dart
// test/core/platform/app_control_handler_test.dart
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/core/platform/app_control_handler.dart';

void main() {
  group('AppControlHandler', () {
    late AppControlHandler handler;

    setUp(() {
      // Prevent real MethodChannel calls during tests.
      TestWidgetsFlutterBinding.ensureInitialized();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
        const MethodChannel('openclaw/a2ui'),
        (call) async => null,
      );
      handler = AppControlHandler();
    });

    tearDown(() {
      handler.dispose();
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .setMockMethodCallHandler(
        const MethodChannel('openclaw/a2ui'),
        null,
      );
    });

    test('emits file path when loadFile is called via MethodChannel', () async {
      final paths = <String>[];
      handler.onFileReceived.listen(paths.add);

      // Simulate C# calling loadFile via MethodChannel
      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage(
        'openclaw/a2ui',
        const StandardMethodCodec().encodeMethodCall(
          const MethodCall('loadFile', '/tmp/test.jsonl'),
        ),
        (data) {},
      );

      await Future<void>.delayed(Duration.zero);
      expect(paths, ['/tmp/test.jsonl']);
    });

    test('ignores unknown method calls', () async {
      final paths = <String>[];
      handler.onFileReceived.listen(paths.add);

      await TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
          .handlePlatformMessage(
        'openclaw/a2ui',
        const StandardMethodCodec().encodeMethodCall(
          const MethodCall('unknownMethod', 'data'),
        ),
        (data) {},
      );

      await Future<void>.delayed(Duration.zero);
      expect(paths, isEmpty);
    });
  });
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test test/core/platform/app_control_handler_test.dart`
Expected: FAIL — `app_control_handler.dart` does not exist.

- [ ] **Step 3: Write minimal implementation**

```dart
// lib/core/platform/app_control_handler.dart
import 'dart:async';

import 'package:flutter/services.dart';

/// Listens for file paths sent from the Tizen C# layer via MethodChannel.
///
/// Sends a "ready" signal on construction so the C# side can flush any
/// pending path that arrived before Flutter was initialised.
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

    // Signal the native side that Flutter is ready to receive paths.
    _channel.invokeMethod<void>('ready', null);
  }

  void dispose() => _filePathController.close();
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test test/core/platform/app_control_handler_test.dart`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add lib/core/platform/app_control_handler.dart test/core/platform/app_control_handler_test.dart
git commit -m "feat: add AppControlHandler for MethodChannel file path reception"
```

## Chunk 3: UI Layer Changes (GenUiScenarioSurface + HomeScreen + wiring)

### Task 4: Add dual-mode support to GenUiScenarioSurface

**Files:**
- Modify: `lib/features/home/widgets/genui_scenario_surface.dart`

- [ ] **Step 1: Add named constructors, `_surfaceId` field, and update imports**

At the top of the file, add the import for `FilePayloadSource`:

```dart
import '../../../core/a2ui/file_payload_source.dart';
```

Replace the current constructor and fields (lines 13-21):

```dart
class GenUiScenarioSurface extends StatefulWidget {
  const GenUiScenarioSurface.scenario({
    required A2uiPayloadSource payloadSource,
    required ScenarioEntry scenario,
    super.key,
  })  : _payloadSource = payloadSource,
        _scenario = scenario,
        filePath = null;

  const GenUiScenarioSurface.file({
    required this.filePath,
    super.key,
  })  : _payloadSource = null,
        _scenario = null;

  final A2uiPayloadSource? _payloadSource;
  final ScenarioEntry? _scenario;
  final String? filePath;

  @override
  State<GenUiScenarioSurface> createState() => _GenUiScenarioSurfaceState();
}
```

- [ ] **Step 2: Add `_surfaceId` state field and update `_loadScenario`**

In `_GenUiScenarioSurfaceState`, add the field (after line 30):

```dart
String _surfaceId = 'main';
```

Replace the `_loadScenario` method (lines 62-93):

```dart
Future<void> _loadScenario() async {
  final loadId = ++_loadId;

  _controller.dispose();
  _controller = _createController();

  try {
    final List<A2uiMessage> messages;

    if (widget.filePath != null) {
      messages = await FilePayloadSource().loadFile(widget.filePath!);
    } else {
      messages = await widget._payloadSource!.load(widget._scenario!.id);
    }

    if (!mounted || loadId != _loadId) return;

    final createMsg = messages.whereType<CreateSurface>().firstOrNull;
    final surfaceId = createMsg?.surfaceId
        ?? widget._scenario?.surfaceId
        ?? 'main';
    final newStyle = resolveSurfaceStyle(surfaceId);

    setState(() {
      _surfaceId = surfaceId;
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

- [ ] **Step 3: Update `didUpdateWidget` to be null-safe**

Replace the `didUpdateWidget` method (lines 54-60):

```dart
@override
void didUpdateWidget(covariant GenUiScenarioSurface oldWidget) {
  super.didUpdateWidget(oldWidget);
  if (oldWidget.filePath != widget.filePath ||
      oldWidget._scenario?.id != widget._scenario?.id) {
    _loadScenario();
  }
}
```

- [ ] **Step 4: Update `build()` to use `_surfaceId`**

In the `build` method, replace line 111:

```dart
// Before:
surfaceContext: _controller.contextFor(widget.scenario.surfaceId),
// After:
surfaceContext: _controller.contextFor(_surfaceId),
```

- [ ] **Step 5: Run existing tests**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test`
Expected: All tests pass. (Existing tests don't use `GenUiScenarioSurface` directly.)

- [ ] **Step 6: Commit**

```bash
git add lib/features/home/widgets/genui_scenario_surface.dart
git commit -m "feat: add dual-mode support to GenUiScenarioSurface (scenario + file)"
```

### Task 5: Update HomeScreen to use named constructor

The existing `HomeScreen` code uses the old unnamed constructor. Update `_PreviewPanel` to use the new named constructor.

**Files:**
- Modify: `lib/features/home/home_screen.dart`

- [ ] **Step 1: Update `_PreviewPanel` to use `GenUiScenarioSurface.scenario`**

In `_PreviewPanel.build()`, replace line 330-333:

```dart
// Before:
child: GenUiScenarioSurface(
  scenario: scenario,
  payloadSource: payloadSource,
),
// After:
child: GenUiScenarioSurface.scenario(
  scenario: scenario,
  payloadSource: payloadSource,
),
```

- [ ] **Step 2: Run tests**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test`
Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add lib/features/home/home_screen.dart
git commit -m "refactor: use GenUiScenarioSurface.scenario named constructor"
```

### Task 6: Wire AppControlHandler into OpenclawTvApp and HomeScreen

**Files:**
- Modify: `lib/main.dart`
- Modify: `lib/app/openclaw_tv_app.dart`
- Modify: `lib/features/home/home_screen.dart`

- [ ] **Step 1: Update `OpenclawTvApp` to accept `appControlHandler`**

```dart
// lib/app/openclaw_tv_app.dart
import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../core/a2ui/a2ui_payload_source.dart';
import '../core/a2ui/json_file_payload_source.dart';
import '../core/platform/app_control_handler.dart';
import '../core/theme/app_theme.dart';
import '../features/home/home_screen.dart';

class OpenclawTvApp extends StatelessWidget {
  const OpenclawTvApp({super.key, this.payloadSource, this.appControlHandler});

  final A2uiPayloadSource? payloadSource;
  final AppControlHandler? appControlHandler;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'OpenClaw TV GenUI',
      locale: const Locale('ko', 'KR'),
      supportedLocales: const [Locale('ko', 'KR'), Locale('en', 'US')],
      localizationsDelegates: GlobalMaterialLocalizations.delegates,
      theme: buildAppTheme(),
      home: HomeScreen(
        payloadSource: payloadSource ?? const JsonFilePayloadSource(),
        appControlHandler: appControlHandler,
      ),
    );
  }
}
```

- [ ] **Step 2: Update `HomeScreen` to accept `appControlHandler` and subscribe to Stream**

Add import at top of `lib/features/home/home_screen.dart`:

```dart
import 'dart:async';

import '../../core/platform/app_control_handler.dart';
```

Update `HomeScreen` widget class (lines 7-13):

```dart
class HomeScreen extends StatefulWidget {
  const HomeScreen({required this.payloadSource, this.appControlHandler, super.key});

  final A2uiPayloadSource payloadSource;
  final AppControlHandler? appControlHandler;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}
```

Update `_HomeScreenState` (lines 16-88):

```dart
class _HomeScreenState extends State<HomeScreen> {
  late ScenarioEntry _selectedScenario;
  StreamSubscription<String>? _appControlSub;
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
    // External file mode: full-screen rendering, no scenario rail.
    if (_externalFilePath != null) {
      return Scaffold(
        body: GenUiScenarioSurface.file(
          key: ValueKey(_externalFilePath),
          filePath: _externalFilePath!,
        ),
      );
    }

    // Catalog mode: existing layout.
    final isWideLayout = MediaQuery.sizeOf(context).width >= 720;

    return Scaffold(
      body: DecoratedBox(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            colors: [Color(0xFF07131C), Color(0xFF0D202C), Color(0xFF12302C)],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(32),
            child: isWideLayout
                ? Row(
                    children: [
                      SizedBox(
                        width: 400,
                        child: _ScenarioRail(
                          selectedScenario: _selectedScenario,
                          onSelectScenario: _handleScenarioChanged,
                        ),
                      ),
                      const SizedBox(width: 32),
                      Expanded(
                        child: _PreviewPanel(
                          scenario: _selectedScenario,
                          payloadSource: widget.payloadSource,
                        ),
                      ),
                    ],
                  )
                : Column(
                    children: [
                      SizedBox(
                        height: 330,
                        child: _ScenarioRail(
                          selectedScenario: _selectedScenario,
                          onSelectScenario: _handleScenarioChanged,
                        ),
                      ),
                      const SizedBox(height: 24),
                      Expanded(
                        child: _PreviewPanel(
                          scenario: _selectedScenario,
                          payloadSource: widget.payloadSource,
                        ),
                      ),
                    ],
                  ),
          ),
        ),
      ),
    );
  }

  void _handleScenarioChanged(ScenarioEntry scenario) {
    setState(() {
      _selectedScenario = scenario;
    });
  }

  @override
  void dispose() {
    _appControlSub?.cancel();
    super.dispose();
  }
}
```

- [ ] **Step 3: Update `main.dart`**

```dart
// lib/main.dart
import 'package:flutter/material.dart';

import 'app/openclaw_tv_app.dart';
import 'core/platform/app_control_handler.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  final appControlHandler = AppControlHandler();

  runApp(OpenclawTvApp(
    appControlHandler: appControlHandler,
  ));
}
```

> **Important:** `WidgetsFlutterBinding.ensureInitialized()` must be called before `AppControlHandler()` because the constructor calls `_channel.invokeMethod('ready')`, which requires the binding.

- [ ] **Step 4: Run all tests**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test`
Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add lib/main.dart lib/app/openclaw_tv_app.dart lib/features/home/home_screen.dart
git commit -m "feat: wire AppControlHandler into app for external file mode"
```

## Chunk 4: Tizen .NET Platform Layer

### Task 7: Create Tizen manifest and Runner.cs

These files cannot be unit-tested in Flutter's test harness — they run on the Tizen device only. Verify by building.

**Files:**
- Create: `tizen/tizen-manifest.xml`
- Create: `tizen/Runner.cs`

- [ ] **Step 1: Create `tizen/tizen-manifest.xml`**

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

- [ ] **Step 2: Create `tizen/Runner.cs`**

```csharp
using Tizen.Flutter.Embedding;

namespace OpenClawTvGenUI
{
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

            // Listen for Flutter "ready" signal to flush any pending path.
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

            // Store initial AppControl path (Flutter may not be ready yet).
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
                _pendingPath = filePath;
            }
        }
    }
}
```

> **Note:** The exact `Tizen.Flutter.Embedding` API may differ from this pseudocode. Verify against the actual SDK at build time. Key areas to check: `MethodChannel` constructor, `SetMethodCallHandler` signature, `ReceivedAppControl.ExtraData` access pattern.

- [ ] **Step 3: Commit**

```bash
git add tizen/
git commit -m "feat: add Tizen .NET Runner with AppControl → MethodChannel bridge"
```

## Chunk 5: Verification

### Task 8: Run full test suite and verify catalog mode still works

- [ ] **Step 1: Run all tests**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter test`
Expected: All tests pass.

- [ ] **Step 2: Verify the app builds and catalog mode works**

Run: `cd /Users/yohoho/work/openclaw_tv_genui && flutter build apk --debug 2>&1 | tail -5`
Expected: Build succeeds. (Full Tizen build requires the Tizen SDK which may not be available locally.)

- [ ] **Step 3: Manual smoke test (if possible)**

Launch the app (`flutter run`) and verify:
- Scenario rail appears as before
- Clicking scenarios switches the preview
- No regressions in existing behavior

- [ ] **Step 4: Final commit if any cleanup needed**

```bash
git status
# If clean, no commit needed.
```
