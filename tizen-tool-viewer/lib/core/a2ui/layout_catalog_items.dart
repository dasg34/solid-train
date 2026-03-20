import 'package:flutter/material.dart';
import 'package:genui/genui.dart';
import 'package:json_schema_builder/json_schema_builder.dart';

final _insetSchema = S.object(
  description: 'Adds extra inner padding around a single child widget.',
  properties: {
    'child': A2uiSchemas.componentReference(),
    'all': S.number(
      description: 'Uniform padding on all sides in logical pixels.',
      minimum: 0,
    ),
    'horizontal': S.number(
      description: 'Horizontal padding in logical pixels.',
      minimum: 0,
    ),
    'vertical': S.number(
      description: 'Vertical padding in logical pixels.',
      minimum: 0,
    ),
  },
  required: ['child'],
);

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

final _maxWidthSchema = S.object(
  description: 'Constrains a child to a maximum width.',
  properties: {
    'child': A2uiSchemas.componentReference(),
    'maxWidth': S.number(
      description: 'Maximum width of the wrapped child in logical pixels.',
      minimum: 1,
    ),
  },
  required: ['child', 'maxWidth'],
);

extension type _InsetData.fromMap(JsonMap _json) {
  String get child => _json['child'] as String;
  double? get all => (_json['all'] as num?)?.toDouble();
  double get horizontal => (_json['horizontal'] as num?)?.toDouble() ?? 0;
  double get vertical => (_json['vertical'] as num?)?.toDouble() ?? 0;
}

extension type _WrapData.fromMap(JsonMap _json) {
  Object? get children => _json['children'];
  double get spacing => (_json['spacing'] as num?)?.toDouble() ?? 16;
  double get runSpacing => (_json['runSpacing'] as num?)?.toDouble() ?? 18;
  String? get alignment => _json['alignment'] as String?;
  String? get runAlignment => _json['runAlignment'] as String?;
  String? get crossAlign => _json['crossAlign'] as String?;
}

extension type _MaxWidthData.fromMap(JsonMap _json) {
  String get child => _json['child'] as String;
  double get maxWidth => (_json['maxWidth'] as num?)?.toDouble() ?? 240;
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

final inset = CatalogItem(
  name: 'Inset',
  dataSchema: _insetSchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "Inset",
          "horizontal": 16,
          "vertical": 12,
          "child": "text1"
        },
        {
          "id": "text1",
          "component": "Text",
          "text": "Inset content"
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final insetData = _InsetData.fromMap(itemContext.data as JsonMap);
    final EdgeInsets padding = insetData.all != null
        ? EdgeInsets.all(insetData.all!)
        : EdgeInsets.symmetric(
            horizontal: insetData.horizontal,
            vertical: insetData.vertical,
          );
    return Padding(
      padding: padding,
      child: itemContext.buildChild(insetData.child, itemContext.dataContext),
    );
  },
);

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

final maxWidthBox = CatalogItem(
  name: 'MaxWidth',
  dataSchema: _maxWidthSchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "MaxWidth",
          "maxWidth": 220,
          "child": "card1"
        },
        {
          "id": "text1",
          "component": "Text",
          "text": "A card with a bounded width"
        },
        {
          "id": "card1",
          "component": "Card",
          "child": "text1"
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final data = _MaxWidthData.fromMap(itemContext.data as JsonMap);
    return ConstrainedBox(
      constraints: BoxConstraints(maxWidth: data.maxWidth),
      child: itemContext.buildChild(data.child, itemContext.dataContext),
    );
  },
);
