import 'package:flutter/material.dart';

import 'app/openclaw_tv_app.dart';
import 'core/platform/app_control_handler.dart';

void main() {
  final appControlHandler = AppControlHandler();

  runApp(OpenclawTvApp(
    appControlHandler: appControlHandler,
  ));
}
