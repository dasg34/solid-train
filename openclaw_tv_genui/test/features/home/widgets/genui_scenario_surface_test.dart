import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/features/home/widgets/genui_scenario_surface.dart';

void main() {
  testWidgets('finance surface keeps charts visible with fitted panel layout', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1920, 1080);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final presentationJson = File(
      '${Directory.current.path}/assets/presentation/finance_focus.json',
    ).readAsStringSync();

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: GenUiScenarioSurface.presentationRaw(
            presentationJson: presentationJson,
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('An error occurred'), findsNothing);

    final chartFinder = find.byType(CustomPaint);
    expect(chartFinder, findsAtLeastNWidgets(2));

    final firstChartSize = tester.getSize(chartFinder.first);
    final secondChartSize = tester.getSize(chartFinder.at(1));

    expect(firstChartSize.width, greaterThan(120));
    expect(firstChartSize.height, greaterThan(60));
    expect(secondChartSize.width, greaterThan(120));
    expect(secondChartSize.height, greaterThan(60));
  });

  testWidgets('surface scrolls with keyboard arrow keys', (tester) async {
    tester.view.physicalSize = const Size(1920, 1080);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final presentationJson = File(
      '${Directory.current.path}/assets/presentation/finance_focus.json',
    ).readAsStringSync();

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: GenUiScenarioSurface.presentationRaw(
            presentationJson: presentationJson,
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    final scrollableState = tester.state<ScrollableState>(
      find.byType(Scrollable),
    );
    expect(scrollableState.position.pixels, 0);

    await tester.sendKeyEvent(LogicalKeyboardKey.arrowDown);
    await tester.pumpAndSettle();

    expect(scrollableState.position.pixels, greaterThan(0));

    await tester.sendKeyEvent(LogicalKeyboardKey.arrowUp);
    await tester.pumpAndSettle();

    expect(scrollableState.position.pixels, lessThanOrEqualTo(1));
  });
}
