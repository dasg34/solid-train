import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/layout_catalog_items.dart';

void main() {
  testWidgets('masonry expands the final odd card to fill the row', (
    tester,
  ) async {
    final controller = SurfaceController(
      catalogs: [
        Catalog([
          BasicCatalogItems.card,
          BasicCatalogItems.text,
          masonryFlow,
        ], catalogId: 'test_catalog'),
      ],
    );
    const surfaceId = 'test_surface';

    controller.handleMessage(
      const UpdateComponents(
        surfaceId: surfaceId,
        components: [
          Component(
            id: 'root',
            type: 'Masonry',
            properties: {
              'children': ['card1', 'card2', 'card3'],
              'maxCrossAxisExtent': 200.0,
              'crossAxisSpacing': 12.0,
              'mainAxisSpacing': 12.0,
              'expandOddTail': true,
            },
          ),
          Component(id: 'card1', type: 'Card', properties: {'child': 'text1'}),
          Component(id: 'card2', type: 'Card', properties: {'child': 'text2'}),
          Component(id: 'card3', type: 'Card', properties: {'child': 'text3'}),
          Component(id: 'text1', type: 'Text', properties: {'text': 'First'}),
          Component(id: 'text2', type: 'Text', properties: {'text': 'Second'}),
          Component(id: 'text3', type: 'Text', properties: {'text': 'Third'}),
        ],
      ),
    );
    controller.handleMessage(
      const CreateSurface(surfaceId: surfaceId, catalogId: 'test_catalog'),
    );

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: SizedBox(
            width: 500,
            child: Surface(
              surfaceContext: controller.contextFor(surfaceId),
            ),
          ),
        ),
      ),
    );
    await tester.pumpAndSettle();

    final firstCard = find.ancestor(
      of: find.text('First'),
      matching: find.byType(Card),
    );
    final thirdCard = find.ancestor(
      of: find.text('Third'),
      matching: find.byType(Card),
    );

    final firstWidth = tester.getSize(firstCard).width;
    final thirdWidth = tester.getSize(thirdCard).width;

    expect(thirdWidth, greaterThan(firstWidth * 1.5));
  });
}
