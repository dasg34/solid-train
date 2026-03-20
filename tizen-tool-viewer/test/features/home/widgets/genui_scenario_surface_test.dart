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
    final highMetricOffset = tester.getTopLeft(find.text('고가'));
    final lowMetricOffset = tester.getTopLeft(find.text('저가'));

    expect(firstChartSize.width, greaterThan(120));
    expect(firstChartSize.height, greaterThan(60));
    expect(secondChartSize.width, greaterThan(120));
    expect(secondChartSize.height, greaterThan(60));
    expect((highMetricOffset.dy - lowMetricOffset.dy).abs(), lessThan(16));
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

    final scrollView = tester.widget<SingleChildScrollView>(
      find.byType(SingleChildScrollView),
    );
    final controller = scrollView.controller!;
    expect(controller.position.pixels, 0);

    await tester.sendKeyEvent(LogicalKeyboardKey.arrowDown);
    await tester.pumpAndSettle();

    expect(controller.position.pixels, greaterThan(0));

    await tester.sendKeyEvent(LogicalKeyboardKey.arrowUp);
    await tester.pumpAndSettle();

    expect(controller.position.pixels, lessThanOrEqualTo(1));
  });

  testWidgets('commute surface keeps fact cards in multiple columns', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1280, 720);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);

    final presentationJson = File(
      '${Directory.current.path}/assets/presentation/commute.json',
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

    final transitOffset = tester.getTopLeft(find.text('대중교통'));
    final alternateOffset = tester.getTopLeft(find.text('대체 경로'));
    final vehicleOffset = tester.getTopLeft(find.text('차량'));
    final arrivalOffset = tester.getTopLeft(find.text('도착 목표'));

    expect((transitOffset.dy - alternateOffset.dy).abs(), lessThan(16));
    expect((vehicleOffset.dy - arrivalOffset.dy).abs(), lessThan(16));
    expect((transitOffset.dx - alternateOffset.dx).abs(), greaterThan(120));
    expect((vehicleOffset.dx - arrivalOffset.dx).abs(), greaterThan(120));
  });
}
