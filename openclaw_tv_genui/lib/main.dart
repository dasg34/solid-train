import 'dart:ui';

import 'package:flutter/material.dart';

import 'app/openclaw_tv_app.dart';
import 'core/logging/app_logger.dart';
import 'core/platform/app_control_handler.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  AppLogger.info('main', 'Flutter bindings initialized.');

  FlutterError.onError = (details) {
    AppLogger.error(
      'flutter',
      'Unhandled Flutter framework error.',
      error: details.exception,
      stackTrace: details.stack,
    );
    FlutterError.presentError(details);
  };
  PlatformDispatcher.instance.onError = (error, stackTrace) {
    AppLogger.error(
      'platform',
      'Unhandled asynchronous platform error.',
      error: error,
      stackTrace: stackTrace,
    );
    return false;
  };

  final appControlHandler = AppControlHandler();
  AppLogger.info('main', 'Starting OpenClaw TV app.');

  runApp(OpenclawTvApp(appControlHandler: appControlHandler));
}
