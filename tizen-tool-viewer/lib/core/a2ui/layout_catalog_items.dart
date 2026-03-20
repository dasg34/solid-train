import 'package:flutter/material.dart';
import 'package:flutter_staggered_grid_view/flutter_staggered_grid_view.dart';
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

final _masonrySchema = S.object(
  description:
      'A non-scrollable masonry layout that packs variable-height cards more tightly than Wrap.',
  properties: {
    'children': A2uiSchemas.componentArrayReference(
      description:
          'An explicit list of widget IDs that will be placed into the masonry grid.',
    ),
    'maxCrossAxisExtent': S.number(
      description:
          'Maximum width of each tile before an additional column is added.',
      minimum: 1,
    ),
    'crossAxisSpacing': S.number(
      description: 'Horizontal spacing between columns in logical pixels.',
      minimum: 0,
    ),
    'mainAxisSpacing': S.number(
      description: 'Vertical spacing between tiles in logical pixels.',
      minimum: 0,
    ),
    'expandOddTail': S.boolean(
      description:
          'When true, a lone last tile in a multi-column odd-length layout expands to the full row width.',
    ),
  },
  required: ['children', 'maxCrossAxisExtent'],
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

extension type _MasonryData.fromMap(JsonMap _json) {
  Object? get children => _json['children'];
  double get maxCrossAxisExtent =>
      (_json['maxCrossAxisExtent'] as num?)?.toDouble() ?? 240;
  double get crossAxisSpacing =>
      (_json['crossAxisSpacing'] as num?)?.toDouble() ?? 16;
  double get mainAxisSpacing =>
      (_json['mainAxisSpacing'] as num?)?.toDouble() ?? 16;
  bool get expandOddTail => _json['expandOddTail'] as bool? ?? false;
}

int _masonryCrossAxisCount(
  double availableWidth,
  double maxCrossAxisExtent,
  double crossAxisSpacing,
) {
  if (!availableWidth.isFinite || availableWidth <= 0) {
    return 1;
  }
  final count =
      ((availableWidth + crossAxisSpacing) /
              (maxCrossAxisExtent + crossAxisSpacing))
          .floor();
  return count.clamp(1, 1000);
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

final masonryFlow = CatalogItem(
  name: 'Masonry',
  dataSchema: _masonrySchema,
  exampleData: [
    () => '''
      [
        {
          "id": "root",
          "component": "Masonry",
          "maxCrossAxisExtent": 220,
          "crossAxisSpacing": 12,
          "mainAxisSpacing": 12,
          "expandOddTail": true,
          "children": ["card1", "card2", "card3"]
        },
        {
          "id": "card1",
          "component": "Card",
          "child": "text1"
        },
        {
          "id": "text1",
          "component": "Text",
          "text": "Short"
        },
        {
          "id": "card2",
          "component": "Card",
          "child": "text2"
        },
        {
          "id": "text2",
          "component": "Text",
          "text": "A taller card with more lines of content for masonry packing."
        },
        {
          "id": "card3",
          "component": "Card",
          "child": "text3"
        },
        {
          "id": "text3",
          "component": "Text",
          "text": "Medium"
        }
      ]
    ''',
  ],
  widgetBuilder: (itemContext) {
    final masonryData = _MasonryData.fromMap(itemContext.data as JsonMap);
    final childIds = (masonryData.children as List<Object?>?)
        ?.map((child) => child.toString())
        .toList();
    if (childIds == null || childIds.isEmpty) {
      return const SizedBox.shrink();
    }

    Widget buildGrid(List<String> ids) {
      return MasonryGridView.extent(
        shrinkWrap: true,
        physics: const NeverScrollableScrollPhysics(),
        padding: EdgeInsets.zero,
        maxCrossAxisExtent: masonryData.maxCrossAxisExtent,
        mainAxisSpacing: masonryData.mainAxisSpacing,
        crossAxisSpacing: masonryData.crossAxisSpacing,
        itemCount: ids.length,
        itemBuilder: (context, index) {
          return itemContext.buildChild(
            ids[index],
            itemContext.dataContext,
          );
        },
      );
    }

    return LayoutBuilder(
      builder: (context, constraints) {
        final crossAxisCount = _masonryCrossAxisCount(
          constraints.maxWidth,
          masonryData.maxCrossAxisExtent,
          masonryData.crossAxisSpacing,
        );
        final shouldExpandOddTail =
            masonryData.expandOddTail &&
            crossAxisCount > 1 &&
            childIds.length > 1 &&
            childIds.length.isOdd;

        if (!shouldExpandOddTail) {
          return buildGrid(childIds);
        }

        final leadingIds = childIds.sublist(0, childIds.length - 1);
        final tailId = childIds.last;

        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            buildGrid(leadingIds),
            SizedBox(height: masonryData.mainAxisSpacing),
            itemContext.buildChild(tailId, itemContext.dataContext),
          ],
        );
      },
    );
  },
);
