enum PresentationChartKind {
  line,
  bar;

  static PresentationChartKind fromJsonValue(Object? value) {
    return switch (value) {
      'line' => PresentationChartKind.line,
      'bar' => PresentationChartKind.bar,
      _ => throw FormatException('chart.kind must be either "line" or "bar".'),
    };
  }
}

class PresentationTheme {
  const PresentationTheme({
    required this.domain,
    required this.pattern,
    this.scale,
  });

  factory PresentationTheme.fromJson(Map<String, Object?> json) {
    return PresentationTheme(
      domain: _readRequiredString(json, 'domain', context: 'theme'),
      pattern: _readRequiredString(json, 'pattern', context: 'theme'),
      scale: _readOptionalString(json, 'scale'),
    );
  }

  final String domain;
  final String pattern;
  final String? scale;
}

class PresentationHero {
  const PresentationHero({
    required this.label,
    required this.value,
    this.detail,
    this.caption,
  });

  factory PresentationHero.fromJson(Map<String, Object?> json) {
    return PresentationHero(
      label: _readRequiredString(json, 'label', context: 'hero'),
      value: _readRequiredString(json, 'value', context: 'hero'),
      detail: _readOptionalString(json, 'detail'),
      caption: _readOptionalString(json, 'caption'),
    );
  }

  final String label;
  final String value;
  final String? detail;
  final String? caption;
}

class PresentationMetric {
  const PresentationMetric({
    required this.label,
    required this.value,
    this.detail,
  });

  factory PresentationMetric.fromJson(
    Map<String, Object?> json, {
    required String context,
  }) {
    return PresentationMetric(
      label: _readRequiredString(json, 'label', context: context),
      value: _readRequiredString(json, 'value', context: context),
      detail: _readOptionalString(json, 'detail'),
    );
  }

  final String label;
  final String value;
  final String? detail;
}

class PresentationChart {
  const PresentationChart({
    required this.title,
    required this.kind,
    required this.values,
    this.labels = const [],
    this.unitLabel,
    this.detail,
  });

  factory PresentationChart.fromJson(Map<String, Object?> json) {
    final labels = _readStringList(json, 'labels');
    final values = _readNumberList(json, 'values', context: 'chart');

    if (values.length < 2) {
      throw const FormatException(
        'chart.values must contain at least 2 points.',
      );
    }
    if (labels.isNotEmpty && labels.length != values.length) {
      throw const FormatException(
        'chart.labels length must match chart.values length.',
      );
    }

    return PresentationChart(
      title: _readRequiredString(json, 'title', context: 'chart'),
      kind: PresentationChartKind.fromJsonValue(json['kind']),
      values: values,
      labels: labels,
      unitLabel: _readOptionalString(json, 'unitLabel'),
      detail: _readOptionalString(json, 'detail'),
    );
  }

  final String title;
  final PresentationChartKind kind;
  final List<double> values;
  final List<String> labels;
  final String? unitLabel;
  final String? detail;
}

class PresentationAlert {
  const PresentationAlert({
    required this.title,
    required this.summary,
    this.meta,
  });

  factory PresentationAlert.fromJson(Map<String, Object?> json) {
    return PresentationAlert(
      title: _readRequiredString(json, 'title', context: 'alert'),
      summary: _readRequiredString(json, 'summary', context: 'alert'),
      meta: _readOptionalString(json, 'meta'),
    );
  }

  final String title;
  final String summary;
  final String? meta;
}

class PresentationSurface {
  const PresentationSurface({
    required this.surfaceId,
    required this.theme,
    required this.title,
    required this.hero,
    this.summary,
    this.metrics = const [],
    this.facts = const [],
    this.chart,
    this.alert,
  });

  factory PresentationSurface.fromJson(Map<String, Object?> json) {
    final chartJson = _readOptionalObject(json, 'chart');
    final alertJson = _readOptionalObject(json, 'alert');

    return PresentationSurface(
      surfaceId: _readRequiredString(json, 'surfaceId', context: 'root'),
      theme: PresentationTheme.fromJson(
        _readRequiredObject(json, 'theme', context: 'root'),
      ),
      title: _readRequiredString(json, 'title', context: 'root'),
      summary: _readOptionalString(json, 'summary'),
      hero: PresentationHero.fromJson(
        _readRequiredObject(json, 'hero', context: 'root'),
      ),
      metrics: _readObjectList(json, 'metrics')
          .map(
            (metric) =>
                PresentationMetric.fromJson(metric, context: 'metrics[]'),
          )
          .toList(growable: false),
      facts: _readObjectList(json, 'facts')
          .map((fact) => PresentationMetric.fromJson(fact, context: 'facts[]'))
          .toList(growable: false),
      chart: chartJson == null ? null : PresentationChart.fromJson(chartJson),
      alert: alertJson == null ? null : PresentationAlert.fromJson(alertJson),
    );
  }

  final String surfaceId;
  final PresentationTheme theme;
  final String title;
  final String? summary;
  final PresentationHero hero;
  final List<PresentationMetric> metrics;
  final List<PresentationMetric> facts;
  final PresentationChart? chart;
  final PresentationAlert? alert;
}

String _readRequiredString(
  Map<String, Object?> json,
  String key, {
  required String context,
}) {
  final value = _readOptionalString(json, key);
  if (value == null || value.isEmpty) {
    throw FormatException('$context.$key is required.');
  }
  return value;
}

String? _readOptionalString(Map<String, Object?> json, String key) {
  final value = json[key];
  if (value == null) {
    return null;
  }
  if (value is! String) {
    throw FormatException('$key must be a string.');
  }
  final trimmed = value.trim();
  return trimmed.isEmpty ? null : trimmed;
}

Map<String, Object?> _readRequiredObject(
  Map<String, Object?> json,
  String key, {
  required String context,
}) {
  final value = _readOptionalObject(json, key);
  if (value == null) {
    throw FormatException('$context.$key must be an object.');
  }
  return value;
}

Map<String, Object?>? _readOptionalObject(
  Map<String, Object?> json,
  String key,
) {
  final value = json[key];
  if (value == null) {
    return null;
  }
  if (value is! Map) {
    throw FormatException('$key must be an object.');
  }
  return Map<String, Object?>.from(value);
}

List<Map<String, Object?>> _readObjectList(
  Map<String, Object?> json,
  String key,
) {
  final value = json[key];
  if (value == null) {
    return const [];
  }
  if (value is! List<Object?>) {
    throw FormatException('$key must be a list.');
  }
  return value
      .map((item) {
        if (item is! Map) {
          throw FormatException('$key items must be objects.');
        }
        return Map<String, Object?>.from(item);
      })
      .toList(growable: false);
}

List<String> _readStringList(Map<String, Object?> json, String key) {
  final value = json[key];
  if (value == null) {
    return const [];
  }
  if (value is! List<Object?>) {
    throw FormatException('$key must be a list of strings.');
  }
  return value
      .map((item) {
        if (item is! String) {
          throw FormatException('$key must contain only strings.');
        }
        final trimmed = item.trim();
        if (trimmed.isEmpty) {
          throw FormatException('$key must not contain empty strings.');
        }
        return trimmed;
      })
      .toList(growable: false);
}

List<double> _readNumberList(
  Map<String, Object?> json,
  String key, {
  required String context,
}) {
  final value = json[key];
  if (value is! List<Object?>) {
    throw FormatException('$context.$key must be a list of numbers.');
  }
  return value
      .map((item) {
        if (item is! num) {
          throw FormatException('$context.$key must contain only numbers.');
        }
        return item.toDouble();
      })
      .toList(growable: false);
}
