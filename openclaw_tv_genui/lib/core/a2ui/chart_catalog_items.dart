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
    final stats = values.isEmpty
        ? const <_ChartStat>[]
        : <_ChartStat>[
            _ChartStat('최근', _formatChartValue(values.last)),
            _ChartStat('최고', _formatChartValue(values.reduce(math.max))),
            _ChartStat('최저', _formatChartValue(values.reduce(math.min))),
          ];

    return _ChartShell(
      height: height,
      labels: labels,
      stats: stats,
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
          valueLabelColor: theme.colorScheme.onSurface.withValues(alpha: 0.96),
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
    final stats = values.isEmpty
        ? const <_ChartStat>[]
        : <_ChartStat>[
            _ChartStat(
              '최대',
              _formatChartValue(values.reduce(math.max), signed: true),
            ),
            _ChartStat(
              '최소',
              _formatChartValue(values.reduce(math.min), signed: true),
            ),
            _ChartStat(
              '범위',
              _formatChartValue(
                values.reduce(math.max) - values.reduce(math.min),
              ),
            ),
          ];

    return _ChartShell(
      height: height,
      labels: labels,
      stats: stats,
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
          valueLabelColor: theme.colorScheme.onSurface.withValues(alpha: 0.96),
          showSignedValueLabels: values.any((value) => value < 0),
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
    required this.stats,
    required this.showLabels,
    required this.hasData,
    required this.emptyStateLabel,
    required this.chart,
  });

  final double height;
  final List<String> labels;
  final List<_ChartStat> stats;
  final bool showLabels;
  final bool hasData;
  final String emptyStateLabel;
  final Widget chart;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final hasLabels = showLabels && labels.isNotEmpty;
    final hasStats = stats.isNotEmpty;
    final chartHeight = height - (hasStats ? 58 : 0) - (hasLabels ? 30 : 0);

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
          if (hasStats) ...[
            _ChartStatsRow(stats: stats),
            const SizedBox(height: 10),
          ],
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

class _ChartStat {
  const _ChartStat(this.label, this.value);

  final String label;
  final String value;
}

class _ChartStatsRow extends StatelessWidget {
  const _ChartStatsRow({required this.stats});

  final List<_ChartStat> stats;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Row(
      children: stats
          .map(
            (stat) => Expanded(
              child: Container(
                margin: const EdgeInsets.symmetric(horizontal: 3),
                padding: const EdgeInsets.symmetric(
                  horizontal: 10,
                  vertical: 8,
                ),
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(14),
                  color: Colors.white.withValues(alpha: 0.05),
                  border: Border.all(
                    color: Colors.white.withValues(alpha: 0.06),
                  ),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      stat.label,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.textTheme.bodySmall?.copyWith(
                        color: Colors.white.withValues(alpha: 0.64),
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(
                      stat.value,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: theme.textTheme.titleMedium?.copyWith(
                        color: Colors.white.withValues(alpha: 0.95),
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          )
          .toList(),
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
    required this.valueLabelColor,
    required this.showGrid,
  });

  final List<double> values;
  final Color lineColor;
  final Color areaTopColor;
  final Color areaBottomColor;
  final Color gridColor;
  final Color pointColor;
  final Color valueLabelColor;
  final bool showGrid;

  @override
  void paint(Canvas canvas, Size size) {
    if (values.isEmpty || size.isEmpty) {
      return;
    }

    const horizontalPadding = 10.0;
    const labelBandHeight = 22.0;
    const plotTopPadding = labelBandHeight + 10.0;
    const plotBottomPadding = 8.0;
    final plotWidth = math.max(size.width - horizontalPadding * 2, 1.0);
    final plotHeight = math.max(
      size.height - plotTopPadding - plotBottomPadding,
      1.0,
    );

    if (showGrid) {
      final gridPaint = Paint()
        ..color = gridColor
        ..strokeWidth = 1;
      for (var i = 0; i < 4; i++) {
        final y = plotTopPadding + plotHeight * (i / 3);
        canvas.drawLine(
          Offset(horizontalPadding, y),
          Offset(size.width - horizontalPadding, y),
          gridPaint,
        );
      }
    }

    final minValue = values.reduce(math.min);
    final maxValue = values.reduce(math.max);
    final range = math.max(maxValue - minValue, 0.001);

    Offset pointFor(int index) {
      final x = values.length == 1
          ? size.width / 2
          : horizontalPadding + plotWidth * index / (values.length - 1);
      final normalized = (values[index] - minValue) / range;
      final y =
          plotTopPadding + plotHeight - (normalized * (plotHeight - 8)) - 4;
      return Offset(
        x,
        y
            .clamp(plotTopPadding + 4, size.height - plotBottomPadding - 4)
            .toDouble(),
      );
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

    for (var i = 0; i < points.length; i++) {
      _paintChartValueChip(
        canvas,
        size,
        centerX: points[i].dx,
        top: points[i].dy - 24,
        text: _formatChartValue(values[i]),
        textColor: valueLabelColor,
      );
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
        oldDelegate.valueLabelColor != valueLabelColor ||
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
    required this.valueLabelColor,
    required this.showSignedValueLabels,
    required this.showGrid,
  });

  final List<double> values;
  final Color positiveColor;
  final Color negativeColor;
  final Color baselineColor;
  final Color gridColor;
  final Color valueLabelColor;
  final bool showSignedValueLabels;
  final bool showGrid;

  @override
  void paint(Canvas canvas, Size size) {
    if (values.isEmpty || size.isEmpty) {
      return;
    }

    const topPadding = 18.0;
    const bottomPadding = 20.0;
    final plotHeight = math.max(size.height - topPadding - bottomPadding, 1.0);

    final minValue = math.min(0, values.reduce(math.min));
    final maxValue = math.max(0, values.reduce(math.max));
    final range = math.max(maxValue - minValue, 0.001);
    double yFor(double value) =>
        topPadding + plotHeight - ((value - minValue) / range) * plotHeight;

    if (showGrid) {
      final gridPaint = Paint()
        ..color = gridColor
        ..strokeWidth = 1;
      for (var i = 0; i < 4; i++) {
        final y = topPadding + plotHeight * (i / 3);
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

      _paintChartValueChip(
        canvas,
        size,
        centerX: rect.center.dx,
        top: value >= 0 ? rect.top - 24 : rect.bottom + 6,
        text: _formatChartValue(value, signed: showSignedValueLabels),
        textColor: valueLabelColor,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _BarChartPainter oldDelegate) {
    return oldDelegate.values != values ||
        oldDelegate.positiveColor != positiveColor ||
        oldDelegate.negativeColor != negativeColor ||
        oldDelegate.baselineColor != baselineColor ||
        oldDelegate.gridColor != gridColor ||
        oldDelegate.valueLabelColor != valueLabelColor ||
        oldDelegate.showSignedValueLabels != showSignedValueLabels ||
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

String _formatChartValue(double value, {bool signed = false}) {
  final rounded = value == value.roundToDouble();
  final text = rounded ? value.toStringAsFixed(0) : value.toStringAsFixed(1);
  if (signed && value > 0) {
    return '+$text';
  }
  return text;
}

void _paintChartValueChip(
  Canvas canvas,
  Size size, {
  required double centerX,
  required double top,
  required String text,
  required Color textColor,
}) {
  final textPainter = TextPainter(
    text: TextSpan(
      text: text,
      style: TextStyle(
        color: textColor,
        fontSize: 11,
        fontWeight: FontWeight.w700,
        height: 1,
      ),
    ),
    textDirection: TextDirection.ltr,
    maxLines: 1,
  )..layout();

  const horizontalPadding = 7.0;
  const verticalPadding = 4.0;
  final chipWidth = textPainter.width + horizontalPadding * 2;
  final chipHeight = textPainter.height + verticalPadding * 2;
  final left = (centerX - chipWidth / 2).clamp(0.0, size.width - chipWidth);
  final clampedTop = top.clamp(0.0, size.height - chipHeight);
  final rect = RRect.fromRectAndRadius(
    Rect.fromLTWH(left, clampedTop, chipWidth, chipHeight),
    const Radius.circular(999),
  );

  final backgroundPaint = Paint()
    ..color = Colors.black.withValues(alpha: 0.34)
    ..style = PaintingStyle.fill;
  canvas.drawRRect(rect, backgroundPaint);

  textPainter.paint(
    canvas,
    Offset(left + horizontalPadding, clampedTop + verticalPadding),
  );
}
