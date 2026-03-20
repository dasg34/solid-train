import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../core/a2ui/a2ui_payload_source.dart';
import '../core/platform/app_control_handler.dart';
import '../core/presentation/presentation_asset_payload_source.dart';
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
      title: 'tizen-tool-viewer',
      locale: const Locale('ko', 'KR'),
      supportedLocales: const [Locale('ko', 'KR'), Locale('en', 'US')],
      localizationsDelegates: GlobalMaterialLocalizations.delegates,
      theme: buildAppTheme(),
      home: HomeScreen(
        payloadSource: payloadSource ?? PresentationAssetPayloadSource(),
        appControlHandler: appControlHandler,
      ),
    );
  }
}
