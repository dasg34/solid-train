import 'package:flutter_test/flutter_test.dart';
import 'package:genui/genui.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_a2ui_builder.dart';
import 'package:openclaw_tv_genui/core/presentation/presentation_surface.dart';

void main() {
  group('buildPresentationDocument', () {
    test('keeps sparse surfaces compact and hero-led', () {
      final surface = PresentationSurface.fromJson({
        'surfaceId': 'stock_card',
        'theme': {'domain': 'finance', 'pattern': 'centerCard'},
        'title': '삼성전자',
        'hero': {'label': '현재가', 'value': '74,300원', 'detail': '전일 대비 -1.2%'},
      });

      final document = buildPresentationDocument(surface);
      final createSurface =
          document[0]['createSurface']! as Map<String, Object?>;
      final theme = createSurface['theme']! as Map<String, Object?>;
      final updateDataModel =
          document[1]['updateDataModel']! as Map<String, Object?>;
      final value = updateDataModel['value']! as Map<String, Object?>;
      final updateComponents =
          document[2]['updateComponents']! as Map<String, Object?>;
      final components = (updateComponents['components']! as List<Object?>)
          .cast<Map<String, Object?>>();
      final root = components.firstWhere(
        (component) => component['id'] == 'root',
      );

      expect(theme['scale'], 'compact');
      expect(value['heroValue'], '74,300원');
      expect(root['children'], ['heroCard']);
      expect(
        components.where((component) => component['component'] == 'LineChart'),
        isEmpty,
      );
      expect(
        components.where((component) => component['component'] == 'Wrap'),
        isEmpty,
      );
    });

    test('adds deterministic chart and support sections for richer data', () {
      final surface = PresentationSurface.fromJson({
        'surfaceId': 'stock_trend',
        'theme': {'domain': 'finance', 'pattern': 'centerCard'},
        'title': '삼성전자',
        'summary': '장 초반 약세 후 낙폭 축소',
        'hero': {
          'label': '현재가',
          'value': '74,300원',
          'detail': '전일 대비 -1.2%',
          'caption': '오전 9:32 기준',
        },
        'metrics': [
          {'label': '고가', 'value': '75,100원'},
          {'label': '저가', 'value': '73,900원'},
          {'label': '거래 강도', 'value': '보통'},
        ],
        'chart': {
          'title': '장중 추이',
          'kind': 'line',
          'labels': ['09:00', '09:10', '09:20', '09:30'],
          'values': [74200, 73900, 74100, 74300],
          'unitLabel': '가격(원)',
        },
        'facts': [
          {'label': '수급', 'value': '외국인 순매도 전환'},
          {'label': '이슈', 'value': '실적 발표 대기'},
        ],
        'alert': {
          'title': '공용 화면 주의',
          'summary': '개인 계좌 정보는 노출하지 않습니다.',
          'meta': '프라이버시 기본값',
        },
      });

      final document = buildPresentationDocument(surface);
      final createSurface =
          document[0]['createSurface']! as Map<String, Object?>;
      final theme = createSurface['theme']! as Map<String, Object?>;
      final updateDataModel =
          document[1]['updateDataModel']! as Map<String, Object?>;
      final value = updateDataModel['value']! as Map<String, Object?>;
      final updateComponents =
          document[2]['updateComponents']! as Map<String, Object?>;
      final components = (updateComponents['components']! as List<Object?>)
          .cast<Map<String, Object?>>();

      expect(theme['scale'], 'standard');
      expect(value['chartTitle'], '장중 추이');
      expect(value['chartValues'], [74200.0, 73900.0, 74100.0, 74300.0]);
      expect(
        components.any((component) => component['component'] == 'LineChart'),
        isTrue,
      );
      expect(
        components
            .where((component) => component['component'] == 'Wrap')
            .length,
        2,
      );

      final messages = buildPresentationMessages(surface);
      expect(messages, hasLength(3));
      expect(messages[0], isA<CreateSurface>());
      expect(messages[1], isA<UpdateDataModel>());
      expect(messages[2], isA<UpdateComponents>());
    });

    test('tightens hero card typography and spacing for side panels', () {
      final surface = PresentationSurface.fromJson({
        'surfaceId': 'commute',
        'theme': {'domain': 'commute', 'pattern': 'sidePanel'},
        'title': '출근길 브리핑',
        'summary': '추천 출발 시각을 먼저 보여줍니다.',
        'hero': {
          'label': '추천 출발',
          'value': '08:10',
          'detail': '지금부터 12분 후',
        },
      });

      final document = buildPresentationDocument(surface);
      final updateComponents =
          document[2]['updateComponents']! as Map<String, Object?>;
      final components = (updateComponents['components']! as List<Object?>)
          .cast<Map<String, Object?>>();

      final heroTitleText = components.firstWhere(
        (component) => component['id'] == 'heroTitleText',
      );
      final heroSummaryText = components.firstWhere(
        (component) => component['id'] == 'heroSummaryText',
      );
      final heroSummaryInset = components.firstWhere(
        (component) => component['id'] == 'heroSummaryInset',
      );
      final heroMetricLabelText = components.firstWhere(
        (component) => component['id'] == 'heroMetricLabelText',
      );
      final heroMetricValueText = components.firstWhere(
        (component) => component['id'] == 'heroMetricValueText',
      );
      final heroMetricDetailText = components.firstWhere(
        (component) => component['id'] == 'heroMetricDetailText',
      );
      final heroMetricInset = components.firstWhere(
        (component) => component['id'] == 'heroMetricInset',
      );
      final heroInset = components.firstWhere(
        (component) => component['id'] == 'heroInset',
      );

      expect(heroTitleText['variant'], 'h2');
      expect(heroSummaryText['variant'], 'body');
      expect(heroSummaryInset['vertical'], 6);
      expect(heroMetricLabelText['variant'], 'caption');
      expect(heroMetricValueText['variant'], 'h2');
      expect(heroMetricDetailText['variant'], 'body');
      expect(heroMetricInset['vertical'], 10);
      expect(heroInset['all'], 22);
      expect(
        components.any((component) => component['id'] == 'heroDivider'),
        isFalse,
      );
    });
  });
}
