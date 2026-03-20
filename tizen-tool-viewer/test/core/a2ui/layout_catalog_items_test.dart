import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/a2ui/layout_catalog_items.dart';

void main() {
  testWidgets('balanced wrap shares remaining row width evenly', (tester) async {
    final controller = SurfaceController(
      catalogs: [
        Catalog([
          BasicCatalogItems.card,
          BasicCatalogItems.text,
          balancedWrap,
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
            type: 'BalancedWrap',
            properties: {
              'children': ['card1', 'card2', 'card3', 'card4', 'card5'],
              'maxCrossAxisExtent': 150.0,
              'crossAxisSpacing': 12.0,
              'mainAxisSpacing': 12.0,
            },
          ),
          Component(id: 'card1', type: 'Card', properties: {'child': 'text1'}),
          Component(id: 'card2', type: 'Card', properties: {'child': 'text2'}),
          Component(id: 'card3', type: 'Card', properties: {'child': 'text3'}),
          Component(id: 'card4', type: 'Card', properties: {'child': 'text4'}),
          Component(id: 'card5', type: 'Card', properties: {'child': 'text5'}),
          Component(id: 'text1', type: 'Text', properties: {'text': 'First'}),
          Component(id: 'text2', type: 'Text', properties: {'text': 'Second'}),
          Component(id: 'text3', type: 'Text', properties: {'text': 'Third'}),
          Component(id: 'text4', type: 'Text', properties: {'text': 'Fourth'}),
          Component(id: 'text5', type: 'Text', properties: {'text': 'Fifth'}),
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
    final fourthCard = find.ancestor(
      of: find.text('Fourth'),
      matching: find.byType(Card),
    );
    final fifthCard = find.ancestor(
      of: find.text('Fifth'),
      matching: find.byType(Card),
    );

    final firstWidth = tester.getSize(firstCard).width;
    final fourthWidth = tester.getSize(fourthCard).width;
    final fifthWidth = tester.getSize(fifthCard).width;

    expect(firstWidth, greaterThan(150));
    expect(fourthWidth, greaterThan(firstWidth));
    expect(fifthWidth, closeTo(fourthWidth, 1.0));
  });
}
