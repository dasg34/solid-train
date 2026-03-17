import 'package:flutter/material.dart';
import 'package:genui/genui.dart';
import 'package:json_schema_builder/json_schema_builder.dart';

final _wrapSchema = S.object(
  description: 'A flowing layout that wraps cards onto new rows as needed.',
  properties: {
    'children': A2uiSchemas.componentArrayReference(
      description:
          'Either an explicit list of widget IDs for the children, or a '
          'template with a data binding to the list of children.',
    ),
    'spacing': S.number(
      description: 'Horizontal spacing between children in a run.',
      minimum: 0,
    ),
    'runSpacing': S.number(
      description: 'Vertical spacing between wrapped runs.',
      minimum: 0,
    ),
    'alignment': S.string(
      enumValues: [
        'start',
        'center',
        'end',
        'spaceBetween',
        'spaceAround',
        'spaceEvenly',
      ],
    ),
    'runAlignment': S.string(
      enumValues: [
        'start',
        'center',
        'end',
        'spaceBetween',
        'spaceAround',
        'spaceEvenly',
      ],
    ),
    'crossAlign': S.string(enumValues: ['start', 'center', 'end']),
  },
  required: ['children'],
);

extension type _WrapData.fromMap(JsonMap _json) {
  Object? get children => _json['children'];
  double get spacing => (_json['spacing'] as num?)?.toDouble() ?? 16;
  double get runSpacing => (_json['runSpacing'] as num?)?.toDouble() ?? 18;
  String? get alignment => _json['alignment'] as String?;
  String? get runAlignment => _json['runAlignment'] as String?;
  String? get crossAlign => _json['crossAlign'] as String?;
}

WrapAlignment _parseWrapAlignment(String? alignment) => switch (alignment) {
  'center' => WrapAlignment.center,
  'end' => WrapAlignment.end,
  'spaceBetween' => WrapAlignment.spaceBetween,
  'spaceAround' => WrapAlignment.spaceAround,
  'spaceEvenly' => WrapAlignment.spaceEvenly,
  _ => WrapAlignment.start,
};

WrapCrossAlignment _parseWrapCrossAlignment(String? alignment) =>
    switch (alignment) {
      'center' => WrapCrossAlignment.center,
      'end' => WrapCrossAlignment.end,
      _ => WrapCrossAlignment.start,
    };

final flowingWrap = CatalogItem(
  name: 'Wrap',
  dataSchema: _wrapSchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "Wrap",
          "spacing": 16,
          "runSpacing": 18,
          "children": ["card1", "card2"]
        },
        {
          "id": "card1",
          "component": "Card",
          "child": "text1"
        },
        {
          "id": "text1",
          "component": "Text",
          "text": "First card"
        },
        {
          "id": "card2",
          "component": "Card",
          "child": "text2"
        },
        {
          "id": "text2",
          "component": "Text",
          "text": "Second card"
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final wrapData = _WrapData.fromMap(itemContext.data as JsonMap);
    final childIds = (wrapData.children as List<Object?>?)
        ?.map((child) => child.toString())
        .toList();
    if (childIds == null || childIds.isEmpty) {
      return const SizedBox.shrink();
    }

    return Wrap(
      spacing: wrapData.spacing,
      runSpacing: wrapData.runSpacing,
      alignment: _parseWrapAlignment(wrapData.alignment),
      runAlignment: _parseWrapAlignment(wrapData.runAlignment),
      crossAxisAlignment: _parseWrapCrossAlignment(wrapData.crossAlign),
      children: childIds
          .map(
            (componentId) =>
                itemContext.buildChild(componentId, itemContext.dataContext),
          )
          .toList(),
    );
  },
);
