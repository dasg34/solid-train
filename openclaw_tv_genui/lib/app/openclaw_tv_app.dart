import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../core/a2ui/a2ui_payload_source.dart';
import '../core/a2ui/json_file_payload_source.dart';
import '../core/theme/app_theme.dart';
import '../features/home/home_screen.dart';

class OpenclawTvApp extends StatelessWidget {
  const OpenclawTvApp({super.key, this.payloadSource});

  final A2uiPayloadSource? payloadSource;

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
      ),
    );
  }
}
