import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:genui/genui.dart';
import 'package:json_schema_builder/json_schema_builder.dart';

final _lineChartSchema = S.object(
  description: 'A compact line chart for trend visualization on TV surfaces.',
  properties: {
    'values': A2uiSchemas.listOrReference(
      items: S.number(),
      description: 'Numeric values to plot in order.',
    ),
    'labels': A2uiSchemas.stringArrayReference(
      description: 'Optional labels aligned with the values.',
    ),
    'height': S.number(
      description: 'Optional chart height in logical pixels.',
      minimum: 96,
    ),
    'strokeColor': S.string(
      description: 'Optional hex color for the line stroke.',
    ),
    'fillStartColor': S.string(
      description: 'Optional hex color for the area fill near the line.',
    ),
    'fillEndColor': S.string(
      description: 'Optional hex color for the area fill near the bottom.',
    ),
    'showGrid': S.boolean(
      description: 'Whether to draw subtle horizontal guide lines.',
    ),
    'showLabels': S.boolean(
      description: 'Whether to show bottom labels under the chart.',
    ),
  },
  required: ['values'],
);

final _barChartSchema = S.object(
  description:
      'A compact bar chart for comparisons or directional movement on TV.',
  properties: {
    'values': A2uiSchemas.listOrReference(
      items: S.number(),
      description: 'Numeric values for each bar.',
    ),
    'labels': A2uiSchemas.stringArrayReference(
      description: 'Optional labels aligned with the bars.',
    ),
    'height': S.number(
      description: 'Optional chart height in logical pixels.',
      minimum: 96,
    ),
    'positiveColor': S.string(
      description: 'Optional hex color for positive bars.',
    ),
    'negativeColor': S.string(
      description: 'Optional hex color for negative bars.',
    ),
    'baselineColor': S.string(
      description: 'Optional hex color for the zero baseline.',
    ),
    'showGrid': S.boolean(
      description: 'Whether to draw subtle horizontal guide lines.',
    ),
    'showLabels': S.boolean(
      description: 'Whether to show bottom labels under the chart.',
    ),
  },
  required: ['values'],
);

extension type _LineChartData.fromMap(JsonMap _json) {
  Object get values => _json['values'] as Object;
  Object? get labels => _json['labels'];
  double get height => (_json['height'] as num?)?.toDouble() ?? 152;
  String? get strokeColor => _json['strokeColor'] as String?;
  String? get fillStartColor => _json['fillStartColor'] as String?;
  String? get fillEndColor => _json['fillEndColor'] as String?;
  bool get showGrid => _json['showGrid'] as bool? ?? true;
  bool get showLabels => _json['showLabels'] as bool? ?? true;
}

extension type _BarChartData.fromMap(JsonMap _json) {
  Object get values => _json['values'] as Object;
  Object? get labels => _json['labels'];
  double get height => (_json['height'] as num?)?.toDouble() ?? 156;
  String? get positiveColor => _json['positiveColor'] as String?;
  String? get negativeColor => _json['negativeColor'] as String?;
  String? get baselineColor => _json['baselineColor'] as String?;
  bool get showGrid => _json['showGrid'] as bool? ?? true;
  bool get showLabels => _json['showLabels'] as bool? ?? true;
}

final lineChart = CatalogItem(
  name: 'LineChart',
  dataSchema: _lineChartSchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "LineChart",
          "values": [18, 20, 19, 23, 21],
          "labels": ["09시", "12시", "15시", "18시", "21시"]
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final data = _LineChartData.fromMap(itemContext.data as JsonMap);
    return _BoundLineChart(
      dataContext: itemContext.dataContext,
      values: data.values,
      labels: data.labels,
      height: data.height,
      strokeColor: data.strokeColor,
      fillStartColor: data.fillStartColor,
      fillEndColor: data.fillEndColor,
      showGrid: data.showGrid,
      showLabels: data.showLabels,
    );
  },
);

final barChart = CatalogItem(
  name: 'BarChart',
  dataSchema: _barChartSchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "BarChart",
          "values": [1.6, 0.9, -0.4],
          "labels": ["삼성", "현대", "NAVER"]
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final data = _BarChartData.fromMap(itemContext.data as JsonMap);
    return _BoundBarChart(
      dataContext: itemContext.dataContext,
      values: data.values,
      labels: data.labels,
      height: data.height,
      positiveColor: data.positiveColor,
      negativeColor: data.negativeColor,
      baselineColor: data.baselineColor,
      showGrid: data.showGrid,
      showLabels: data.showLabels,
    );
  },
);

class _BoundLineChart extends StatelessWidget {
  const _BoundLineChart({
    required this.dataContext,
    required this.values,
    required this.labels,
    required this.height,
    required this.strokeColor,
    required this.fillStartColor,
    required this.fillEndColor,
    required this.showGrid,
    required this.showLabels,
  });

  final DataContext dataContext;
  final Object values;
  final Object? labels;
  final double height;
  final String? strokeColor;
  final String? fillStartColor;
  final String? fillEndColor;
  final bool showGrid;
  final bool showLabels;

  @override
  Widget build(BuildContext context) {
    return BoundList(
      dataContext: dataContext,
      value: values,
      builder: (context, resolvedValues) {
        return BoundList(
          dataContext: dataContext,
          value: labels ?? const <Object?>[],
          builder: (context, resolvedLabels) {
            return _LineChartPanel(
              values: _coerceNumbers(resolvedValues),
              labels: _coerceStrings(resolvedLabels),
              height: height,
              strokeColor: strokeColor,
              fillStartColor: fillStartColor,
              fillEndColor: fillEndColor,
              showGrid: showGrid,
              showLabels: showLabels,
            );
          },
        );
      },
    );
  }
}

class _BoundBarChart extends StatelessWidget {
  const _BoundBarChart({
    required this.dataContext,
    required this.values,
    required this.labels,
    required this.height,
    required this.positiveColor,
    required this.negativeColor,
    required this.baselineColor,
    required this.showGrid,
    required this.showLabels,
  });

  final DataContext dataContext;
  final Object values;
  final Object? labels;
  final double height;
  final String? positiveColor;
  final String? negativeColor;
  final String? baselineColor;
  final bool showGrid;
  final bool showLabels;

  @override
  Widget build(BuildContext context) {
    return BoundList(
      dataContext: dataContext,
      value: values,
      builder: (context, resolvedValues) {
        return BoundList(
          dataContext: dataContext,
          value: labels ?? const <Object?>[],
          builder: (context, resolvedLabels) {
            return _BarChartPanel(
              values: _coerceNumbers(resolvedValues),
              labels: _coerceStrings(resolvedLabels),
              height: height,
              positiveColor: positiveColor,
              negativeColor: negativeColor,
              baselineColor: baselineColor,
              showGrid: showGrid,
              showLabels: showLabels,
            );
          },
        );
      },
    );
  }
}

class _LineChartPanel extends StatelessWidget {
  const _LineChartPanel({
    required this.values,
    required this.labels,
    required this.height,
    required this.strokeColor,
    required this.fillStartColor,
    required this.fillEndColor,
    required this.showGrid,
    required this.showLabels,
  });

  final List<double> values;
  final List<String> labels;
  final double height;
  final String? strokeColor;
  final String? fillStartColor;
  final String? fillEndColor;
  final bool showGrid;
  final bool showLabels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final lineColor =
        _parseHexColor(strokeColor) ??
        theme.colorScheme.primary.withValues(alpha: 0.92);
    final areaTopColor =
        _parseHexColor(fillStartColor) ??
        theme.colorScheme.primary.withValues(alpha: 0.28);
    final areaBottomColor = _parseHexColor(fillEndColor) ?? Colors.transparent;

    return _ChartShell(
      height: height,
      labels: labels,
      showLabels: showLabels,
      hasData: values.isNotEmpty,
      emptyStateLabel: '차트 데이터 없음',
      chart: CustomPaint(
        painter: _LineChartPainter(
          values: values,
          lineColor: lineColor,
          areaTopColor: areaTopColor,
          areaBottomColor: areaBottomColor,
          gridColor: Colors.white.withValues(alpha: 0.08),
          pointColor: theme.colorScheme.onSurface,
          showGrid: showGrid,
        ),
      ),
    );
  }
}

class _BarChartPanel extends StatelessWidget {
  const _BarChartPanel({
    required this.values,
    required this.labels,
    required this.height,
    required this.positiveColor,
    required this.negativeColor,
    required this.baselineColor,
    required this.showGrid,
    required this.showLabels,
  });

  final List<double> values;
  final List<String> labels;
  final double height;
  final String? positiveColor;
  final String? negativeColor;
  final String? baselineColor;
  final bool showGrid;
  final bool showLabels;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final upColor =
        _parseHexColor(positiveColor) ??
        theme.colorScheme.primary.withValues(alpha: 0.92);
    final downColor =
        _parseHexColor(negativeColor) ??
        theme.colorScheme.error.withValues(alpha: 0.90);
    final zeroColor =
        _parseHexColor(baselineColor) ?? Colors.white.withValues(alpha: 0.18);

    return _ChartShell(
      height: height,
      labels: labels,
      showLabels: showLabels,
      hasData: values.isNotEmpty,
      emptyStateLabel: '비교 데이터 없음',
      chart: CustomPaint(
        painter: _BarChartPainter(
          values: values,
          positiveColor: upColor,
          negativeColor: downColor,
          baselineColor: zeroColor,
          gridColor: Colors.white.withValues(alpha: 0.08),
          showGrid: showGrid,
        ),
      ),
    );
  }
}

class _ChartShell extends StatelessWidget {
  const _ChartShell({
    required this.height,
    required this.labels,
    required this.showLabels,
    required this.hasData,
    required this.emptyStateLabel,
    required this.chart,
  });

  final double height;
  final List<String> labels;
  final bool showLabels;
  final bool hasData;
  final String emptyStateLabel;
  final Widget chart;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final hasLabels = showLabels && labels.isNotEmpty;
    final chartHeight = hasLabels ? height - 30 : height;

    return Container(
      width: double.infinity,
      height: height,
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 10),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(22),
        color: Colors.white.withValues(alpha: 0.03),
        border: Border.all(color: Colors.white.withValues(alpha: 0.06)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Expanded(
            child: chartHeight <= 24
                ? const SizedBox.shrink()
                : ClipRRect(
                    borderRadius: BorderRadius.circular(16),
                    child: chart,
                  ),
          ),
          if (hasLabels) ...[
            const SizedBox(height: 8),
            SizedBox(
              height: 22,
              child: Row(
                children: labels
                    .map(
                      (label) => Expanded(
                        child: Text(
                          label,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          textAlign: TextAlign.center,
                          style: theme.textTheme.bodySmall?.copyWith(
                            color: Colors.white.withValues(alpha: 0.72),
                            fontWeight: FontWeight.w500,
                          ),
                        ),
                      ),
                    )
                    .toList(),
              ),
            ),
          ],
          if (!hasData) ...[
            const SizedBox(height: 8),
            Text(
              emptyStateLabel,
              textAlign: TextAlign.center,
              style: theme.textTheme.bodySmall?.copyWith(
                color: Colors.white.withValues(alpha: 0.60),
              ),
            ),
          ],
        ],
      ),
    );
  }
}

class _LineChartPainter extends CustomPainter {
  const _LineChartPainter({
    required this.values,
    required this.lineColor,
    required this.areaTopColor,
    required this.areaBottomColor,
    required this.gridColor,
    required this.pointColor,
    required this.showGrid,
  });

  final List<double> values;
  final Color lineColor;
  final Color areaTopColor;
  final Color areaBottomColor;
  final Color gridColor;
  final Color pointColor;
  final bool showGrid;

  @override
  void paint(Canvas canvas, Size size) {
    if (values.isEmpty || size.isEmpty) {
      return;
    }

    if (showGrid) {
      final gridPaint = Paint()
        ..color = gridColor
        ..strokeWidth = 1;
      for (var i = 0; i < 4; i++) {
        final y = size.height * (i / 3);
        canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
      }
    }

    final minValue = values.reduce(math.min);
    final maxValue = values.reduce(math.max);
    final range = math.max(maxValue - minValue, 0.001);

    Offset pointFor(int index) {
      final x = values.length == 1
          ? size.width / 2
          : size.width * index / (values.length - 1);
      final normalized = (values[index] - minValue) / range;
      final y = size.height - (normalized * (size.height - 8)) - 4;
      return Offset(x, y.clamp(4, size.height - 4).toDouble());
    }

    final points = List<Offset>.generate(values.length, pointFor);
    final strokePath = Path()..moveTo(points.first.dx, points.first.dy);

    if (points.length == 1) {
      strokePath.lineTo(points.first.dx, points.first.dy);
    } else {
      for (var i = 1; i < points.length; i++) {
        final previous = points[i - 1];
        final current = points[i];
        final control = Offset((previous.dx + current.dx) / 2, previous.dy);
        final control2 = Offset((previous.dx + current.dx) / 2, current.dy);
        strokePath.cubicTo(
          control.dx,
          control.dy,
          control2.dx,
          control2.dy,
          current.dx,
          current.dy,
        );
      }
    }

    final fillPath = Path.from(strokePath)
      ..lineTo(points.last.dx, size.height)
      ..lineTo(points.first.dx, size.height)
      ..close();

    final fillPaint = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topCenter,
        end: Alignment.bottomCenter,
        colors: [areaTopColor, areaBottomColor],
      ).createShader(Offset.zero & size);
    canvas.drawPath(fillPath, fillPaint);

    final strokePaint = Paint()
      ..color = lineColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3
      ..strokeCap = StrokeCap.round
      ..strokeJoin = StrokeJoin.round;
    canvas.drawPath(strokePath, strokePaint);

    final pointPaint = Paint()..color = pointColor;
    final haloPaint = Paint()
      ..color = lineColor.withValues(alpha: 0.24)
      ..style = PaintingStyle.fill;

    for (final point in points) {
      canvas.drawCircle(point, 4.5, haloPaint);
      canvas.drawCircle(point, 2.2, pointPaint);
    }
  }

  @override
  bool shouldRepaint(covariant _LineChartPainter oldDelegate) {
    return oldDelegate.values != values ||
        oldDelegate.lineColor != lineColor ||
        oldDelegate.areaTopColor != areaTopColor ||
        oldDelegate.areaBottomColor != areaBottomColor ||
        oldDelegate.gridColor != gridColor ||
        oldDelegate.pointColor != pointColor ||
        oldDelegate.showGrid != showGrid;
  }
}

class _BarChartPainter extends CustomPainter {
  const _BarChartPainter({
    required this.values,
    required this.positiveColor,
    required this.negativeColor,
    required this.baselineColor,
    required this.gridColor,
    required this.showGrid,
  });

  final List<double> values;
  final Color positiveColor;
  final Color negativeColor;
  final Color baselineColor;
  final Color gridColor;
  final bool showGrid;

  @override
  void paint(Canvas canvas, Size size) {
    if (values.isEmpty || size.isEmpty) {
      return;
    }

    final minValue = math.min(0, values.reduce(math.min));
    final maxValue = math.max(0, values.reduce(math.max));
    final range = math.max(maxValue - minValue, 0.001);
    double yFor(double value) =>
        size.height - ((value - minValue) / range) * size.height;

    if (showGrid) {
      final gridPaint = Paint()
        ..color = gridColor
        ..strokeWidth = 1;
      for (var i = 0; i < 4; i++) {
        final y = size.height * (i / 3);
        canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
      }
    }

    final baselineY = yFor(0);
    final baselinePaint = Paint()
      ..color = baselineColor
      ..strokeWidth = 1.2;
    canvas.drawLine(
      Offset(0, baselineY),
      Offset(size.width, baselineY),
      baselinePaint,
    );

    final count = values.length;
    final gap = count <= 4 ? 14.0 : 10.0;
    final slotWidth = (size.width - gap * (count - 1)) / count;
    final barWidth = math.min(slotWidth, 26.0).toDouble();

    for (var i = 0; i < count; i++) {
      final value = values[i];
      final x = i * (slotWidth + gap) + ((slotWidth - barWidth) / 2);
      final valueY = yFor(value);
      final top = math.min(baselineY, valueY);
      final bottom = math.max(baselineY, valueY);
      final rect = RRect.fromRectAndRadius(
        Rect.fromLTRB(x, top, x + barWidth, bottom == top ? top + 2 : bottom),
        const Radius.circular(8),
      );
      final paint = Paint()..color = value >= 0 ? positiveColor : negativeColor;
      canvas.drawRRect(rect, paint);
    }
  }

  @override
  bool shouldRepaint(covariant _BarChartPainter oldDelegate) {
    return oldDelegate.values != values ||
        oldDelegate.positiveColor != positiveColor ||
        oldDelegate.negativeColor != negativeColor ||
        oldDelegate.baselineColor != baselineColor ||
        oldDelegate.gridColor != gridColor ||
        oldDelegate.showGrid != showGrid;
  }
}

List<double> _coerceNumbers(List<Object?>? values) {
  if (values == null) return const [];
  return values
      .map((value) {
        if (value is num) return value.toDouble();
        if (value is String) return double.tryParse(value);
        return null;
      })
      .whereType<double>()
      .toList();
}

List<String> _coerceStrings(List<Object?>? values) {
  if (values == null) return const [];
  return values
      .map((value) => value?.toString())
      .whereType<String>()
      .where((value) => value.isNotEmpty)
      .toList();
}

Color? _parseHexColor(String? value) {
  if (value == null || value.isEmpty) {
    return null;
  }

  final normalized = value.replaceAll('#', '');
  if (normalized.length != 6 && normalized.length != 8) {
    return null;
  }

  final hexValue = normalized.length == 6 ? 'FF$normalized' : normalized;
  final parsed = int.tryParse(hexValue, radix: 16);
  if (parsed == null) {
    return null;
  }
  return Color(parsed);
}
