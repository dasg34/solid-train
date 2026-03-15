import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../core/theme/app_theme.dart';
import '../features/home/home_screen.dart';
import '../features/home/models/template_registry.dart';

class OpenclawTvApp extends StatelessWidget {
  const OpenclawTvApp({super.key, this.templateRegistry});

  final TemplateRegistry? templateRegistry;

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
        templateRegistry: templateRegistry ?? buildDefaultTemplateRegistry(),
      ),
    );
  }
}
