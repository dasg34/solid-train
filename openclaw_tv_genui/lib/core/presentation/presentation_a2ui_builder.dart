import 'dart:convert';

import 'package:genui/genui.dart';

import '../a2ui/parse_ndjson.dart';
import 'presentation_surface.dart';

const presentationCatalogId =
    'https://a2ui.org/specification/v0_9/standard_catalog.json';

List<Map<String, Object?>> buildPresentationDocument(
  PresentationSurface surface,
) {
  return _PresentationA2uiBuilder(surface).build();
}

List<A2uiMessage> buildPresentationMessages(PresentationSurface surface) {
  final ndjson = buildPresentationDocument(surface).map(jsonEncode).join('\n');
  return parseNdjson(ndjson);
}

class _PresentationA2uiBuilder {
  _PresentationA2uiBuilder(this.surface);

  final PresentationSurface surface;
  final List<Map<String, Object?>> _components = [];
  final Map<String, Object?> _data = {};
  final List<String> _rootChildren = [];

  bool get _isSidePanel => surface.theme.pattern == 'sidePanel';
  double get _smallCardInsetAll => _isSidePanel ? 16 : 20;
  double get _featureCardInsetAll => _isSidePanel ? 18 : 24;

  List<Map<String, Object?>> build() {
    _bindSurfaceData();

    _rootChildren.add(_buildHeroCard());

    if (surface.metrics.isNotEmpty) {
      _rootChildren.add(_buildMetricWrap());
    }
    if (surface.chart != null) {
      _rootChildren.add(_buildChartCard(surface.chart!));
    }
    if (surface.facts.isNotEmpty) {
      _rootChildren.add(_buildFactWrap());
    }
    if (surface.alert != null) {
      _rootChildren.add(_buildAlertCard(surface.alert!));
    }

    _components.insert(0, {
      'id': 'root',
      'component': 'Column',
      'children': _rootChildren,
      'align': 'stretch',
    });

    return [
      {
        'version': 'v0.9',
        'createSurface': {
          'surfaceId': surface.surfaceId,
          'catalogId': presentationCatalogId,
          'theme': {
            'domain': surface.theme.domain,
            'pattern': surface.theme.pattern,
            'scale': surface.theme.scale ?? _defaultScale(),
          },
        },
      },
      {
        'version': 'v0.9',
        'updateDataModel': {'surfaceId': surface.surfaceId, 'value': _data},
      },
      {
        'version': 'v0.9',
        'updateComponents': {
          'surfaceId': surface.surfaceId,
          'components': _components,
        },
      },
    ];
  }

  void _bindSurfaceData() {
    _data['title'] = surface.title;
    _data['heroLabel'] = surface.hero.label;
    _data['heroValue'] = surface.hero.value;

    if (surface.summary case final summary?) {
      _data['summary'] = summary;
    }
    if (surface.hero.detail case final detail?) {
      _data['heroDetail'] = detail;
    }
    if (surface.hero.caption case final caption?) {
      _data['heroCaption'] = caption;
    }

    for (var i = 0; i < surface.metrics.length; i++) {
      final metric = surface.metrics[i];
      final index = i + 1;
      _data['metric${index}Label'] = metric.label;
      _data['metric${index}Value'] = metric.value;
      if (metric.detail case final detail?) {
        _data['metric${index}Detail'] = detail;
      }
    }

    for (var i = 0; i < surface.facts.length; i++) {
      final fact = surface.facts[i];
      final index = i + 1;
      _data['fact${index}Label'] = fact.label;
      _data['fact${index}Value'] = fact.value;
      if (fact.detail case final detail?) {
        _data['fact${index}Detail'] = detail;
      }
    }

    if (surface.chart case final chart?) {
      _data['chartTitle'] = chart.title;
      _data['chartValues'] = chart.values;
      if (chart.labels.isNotEmpty) {
        _data['chartLabels'] = chart.labels;
      }
      if (chart.unitLabel case final unitLabel?) {
        _data['chartUnitLabel'] = unitLabel;
      }
      if (chart.detail case final detail?) {
        _data['chartDetail'] = detail;
      }
    }

    if (surface.alert case final alert?) {
      _data['alertTitle'] = alert.title;
      _data['alertSummary'] = alert.summary;
      if (alert.meta case final meta?) {
        _data['alertMeta'] = meta;
      }
    }
  }

  String _defaultScale() {
    final hasDenseContent =
        surface.metrics.length >= 3 ||
        surface.facts.length >= 3 ||
        surface.chart != null ||
        surface.alert != null;
    return hasDenseContent ? 'standard' : 'compact';
  }

  String _buildHeroCard() {
    _addComponent(
      id: 'heroTitleText',
      component: 'Text',
      props: {
        'text': _path('title'),
        'variant': _isSidePanel ? 'h2' : 'h1',
      },
    );

    final heroChildren = <String>['heroTitleText'];

    if (_data.containsKey('summary')) {
      _addComponent(
        id: 'heroSummaryText',
        component: 'Text',
        props: {
          'text': _path('summary'),
          'variant': _isSidePanel ? 'body' : 'h4',
        },
      );
      _addComponent(
        id: 'heroSummaryInset',
        component: 'Inset',
        props: {
          'child': 'heroSummaryText',
          'vertical': _isSidePanel ? 6 : 10,
        },
      );
      heroChildren.add('heroSummaryInset');
      _addComponent(id: 'heroDivider', component: 'Divider', props: const {});
      _addComponent(
        id: 'heroDividerInset',
        component: 'Inset',
        props: {
          'child': 'heroDivider',
          'vertical': _isSidePanel ? 4 : 8,
        },
      );
      heroChildren.add('heroDividerInset');
    }

    _addComponent(
      id: 'heroMetricLabelText',
      component: 'Text',
      props: {
        'text': _path('heroLabel'),
        'variant': _isSidePanel ? 'caption' : 'body',
      },
    );
    _addComponent(
      id: 'heroMetricValueText',
      component: 'Text',
      props: {
        'text': _path('heroValue'),
        'variant': _isSidePanel ? 'h2' : 'h1',
      },
    );

    final heroMetricChildren = <String>[
      'heroMetricLabelText',
      'heroMetricValueText',
    ];

    if (_data.containsKey('heroDetail')) {
      _addComponent(
        id: 'heroMetricDetailText',
        component: 'Text',
        props: {
          'text': _path('heroDetail'),
          'variant': _isSidePanel ? 'body' : 'h4',
        },
      );
      heroMetricChildren.add('heroMetricDetailText');
    }

    _addComponent(
      id: 'heroMetricColumn',
      component: 'Column',
      props: {'children': heroMetricChildren},
    );
    _addComponent(
      id: 'heroMetricInset',
      component: 'Inset',
      props: {
        'child': 'heroMetricColumn',
        'vertical': _isSidePanel ? 10 : 14,
      },
    );
    heroChildren.add('heroMetricInset');

    if (_data.containsKey('heroCaption')) {
      _addComponent(
        id: 'heroCaptionText',
        component: 'Text',
        props: {'text': _path('heroCaption'), 'variant': 'caption'},
      );
      heroChildren.add('heroCaptionText');
    }

    _addComponent(
      id: 'heroColumn',
      component: 'Column',
      props: {'children': heroChildren, 'align': 'stretch'},
    );
    _addComponent(
      id: 'heroInset',
      component: 'Inset',
      props: {'child': 'heroColumn', 'all': _isSidePanel ? 22 : 28},
    );
    return _addComponent(
      id: 'heroCard',
      component: 'Card',
      props: {'child': 'heroInset'},
    );
  }

  String _buildMetricWrap() {
    final metricCardIds = <String>[];
    for (var i = 0; i < surface.metrics.length; i++) {
      final index = i + 1;
      final labelId = 'metric${index}LabelText';
      final valueId = 'metric${index}ValueText';
      final columnChildren = <String>[labelId, valueId];

      _addComponent(
        id: labelId,
        component: 'Text',
        props: {'text': _path('metric${index}Label'), 'variant': 'body'},
      );
      _addComponent(
        id: valueId,
        component: 'Text',
        props: {'text': _path('metric${index}Value'), 'variant': 'h3'},
      );

      final detailKey = 'metric${index}Detail';
      if (_data.containsKey(detailKey)) {
        final detailId = 'metric${index}DetailText';
        _addComponent(
          id: detailId,
          component: 'Text',
          props: {'text': _path(detailKey), 'variant': 'caption'},
        );
        columnChildren.add(detailId);
      }

      final columnId = 'metric${index}Column';
      final insetId = 'metric${index}Inset';
      final cardId = 'metric${index}Card';

      _addComponent(
        id: columnId,
        component: 'Column',
        props: {'children': columnChildren},
      );
      _addComponent(
        id: insetId,
        component: 'Inset',
        props: {'child': columnId, 'all': _smallCardInsetAll},
      );
      metricCardIds.add(
        _addComponent(id: cardId, component: 'Card', props: {'child': insetId}),
      );
    }

    return _addComponent(
      id: 'metricWrap',
      component: 'Wrap',
      props: {'children': metricCardIds, 'spacing': 18, 'runSpacing': 18},
    );
  }

  String _buildChartCard(PresentationChart chart) {
    final chartChildren = <String>[];

    _addComponent(
      id: 'chartTitleText',
      component: 'Text',
      props: {'text': _path('chartTitle'), 'variant': 'h3'},
    );
    chartChildren.add('chartTitleText');

    if (_data.containsKey('chartUnitLabel')) {
      _addComponent(
        id: 'chartUnitLabelText',
        component: 'Text',
        props: {'text': _path('chartUnitLabel'), 'variant': 'caption'},
      );
      _addComponent(
        id: 'chartUnitLabelInset',
        component: 'Inset',
        props: {'child': 'chartUnitLabelText', 'vertical': 6},
      );
      chartChildren.add('chartUnitLabelInset');
    }

    final chartId = switch (chart.kind) {
      PresentationChartKind.line => _addComponent(
        id: 'trendChart',
        component: 'LineChart',
        props: {
          'values': _path('chartValues'),
          'labels': _data.containsKey('chartLabels')
              ? _path('chartLabels')
              : null,
          'height': 156,
          'strokeColor': '#78E3FF',
          'fillStartColor': '#78E3FF33',
          'fillEndColor': '#00000000',
          'showLabels': true,
        }..removeWhere((key, value) => value == null),
      ),
      PresentationChartKind.bar => _addComponent(
        id: 'trendChart',
        component: 'BarChart',
        props: {
          'values': _path('chartValues'),
          'labels': _data.containsKey('chartLabels')
              ? _path('chartLabels')
              : null,
          'height': 156,
          'positiveColor': '#50D890',
          'negativeColor': '#FF6B6B',
          'baselineColor': '#6C7586',
          'showLabels': true,
        }..removeWhere((key, value) => value == null),
      ),
    };
    chartChildren.add(chartId);

    if (_data.containsKey('chartDetail')) {
      _addComponent(
        id: 'chartDetailText',
        component: 'Text',
        props: {'text': _path('chartDetail'), 'variant': 'caption'},
      );
      _addComponent(
        id: 'chartDetailInset',
        component: 'Inset',
        props: {'child': 'chartDetailText', 'vertical': 8},
      );
      chartChildren.add('chartDetailInset');
    }

    _addComponent(
      id: 'chartColumn',
      component: 'Column',
      props: {'children': chartChildren},
    );
    _addComponent(
      id: 'chartInset',
      component: 'Inset',
      props: {'child': 'chartColumn', 'all': _featureCardInsetAll},
    );
    return _addComponent(
      id: 'chartCard',
      component: 'Card',
      props: {'child': 'chartInset'},
    );
  }

  String _buildFactWrap() {
    final factCardIds = <String>[];
    for (var i = 0; i < surface.facts.length; i++) {
      final index = i + 1;
      final labelId = 'fact${index}LabelText';
      final valueId = 'fact${index}ValueText';
      final children = <String>[labelId, valueId];

      _addComponent(
        id: labelId,
        component: 'Text',
        props: {'text': _path('fact${index}Label'), 'variant': 'body'},
      );
      _addComponent(
        id: valueId,
        component: 'Text',
        props: {'text': _path('fact${index}Value'), 'variant': 'h4'},
      );

      final detailKey = 'fact${index}Detail';
      if (_data.containsKey(detailKey)) {
        final detailId = 'fact${index}DetailText';
        _addComponent(
          id: detailId,
          component: 'Text',
          props: {'text': _path(detailKey), 'variant': 'caption'},
        );
        children.add(detailId);
      }

      final columnId = 'fact${index}Column';
      final insetId = 'fact${index}Inset';
      final cardId = 'fact${index}Card';

      _addComponent(
        id: columnId,
        component: 'Column',
        props: {'children': children},
      );
      _addComponent(
        id: insetId,
        component: 'Inset',
        props: {'child': columnId, 'all': _smallCardInsetAll},
      );
      factCardIds.add(
        _addComponent(id: cardId, component: 'Card', props: {'child': insetId}),
      );
    }

    return _addComponent(
      id: 'factWrap',
      component: 'Wrap',
      props: {'children': factCardIds, 'spacing': 18, 'runSpacing': 18},
    );
  }

  String _buildAlertCard(PresentationAlert alert) {
    _addComponent(
      id: 'alertIcon',
      component: 'Icon',
      props: {'name': 'warning'},
    );
    _addComponent(
      id: 'alertTitleText',
      component: 'Text',
      props: {'text': _path('alertTitle'), 'variant': 'h4'},
    );
    _addComponent(
      id: 'alertHeaderRow',
      component: 'Row',
      props: {
        'children': ['alertIcon', 'alertTitleText'],
        'align': 'center',
      },
    );
    _addComponent(
      id: 'alertSummaryText',
      component: 'Text',
      props: {'text': _path('alertSummary'), 'variant': 'body'},
    );

    final alertChildren = <String>['alertHeaderRow', 'alertSummaryText'];

    if (_data.containsKey('alertMeta')) {
      _addComponent(
        id: 'alertMetaText',
        component: 'Text',
        props: {'text': _path('alertMeta'), 'variant': 'caption'},
      );
      alertChildren.add('alertMetaText');
    }

    _addComponent(
      id: 'alertColumn',
      component: 'Column',
      props: {'children': alertChildren},
    );
    _addComponent(
      id: 'alertInset',
      component: 'Inset',
      props: {'child': 'alertColumn', 'all': _featureCardInsetAll},
    );
    return _addComponent(
      id: 'alertCard',
      component: 'Card',
      props: {'child': 'alertInset'},
    );
  }

  String _addComponent({
    required String id,
    required String component,
    required Map<String, Object?> props,
  }) {
    _components.add({'id': id, 'component': component, ...props});
    return id;
  }

  Map<String, String> _path(String key) => {'path': '/$key'};
}
