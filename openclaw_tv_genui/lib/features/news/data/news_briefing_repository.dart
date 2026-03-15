import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:genui/genui.dart';
import 'package:http/http.dart' as http;
import 'package:xml/xml.dart';

import '../../home/models/template_surface.dart';

abstract interface class NewsBriefingRepository {
  Future<Map<String, Object?>> fetchNewsBriefing({int count = 6});
}

class YonhapNewsBriefingRepository implements NewsBriefingRepository {
  const YonhapNewsBriefingRepository({
    this.feedUrl,
    this.webProxyUrl = 'http://127.0.0.1:3001/yonhap-feed',
  });

  final String? feedUrl;
  final String webProxyUrl;

  @override
  Future<Map<String, Object?>> fetchNewsBriefing({int count = 6}) async {
    final targetUrl = feedUrl ?? _defaultFeedUrl();
    final response = await http
        .get(Uri.parse(targetUrl))
        .timeout(const Duration(seconds: 6));
    if (response.statusCode != 200) {
      throw Exception('뉴스 RSS 응답 실패 (${response.statusCode})');
    }

    final document = XmlDocument.parse(utf8.decode(response.bodyBytes));
    final channel = _firstOrNull(document.findAllElements('channel'));
    if (channel == null) {
      throw Exception('뉴스 RSS channel을 찾지 못했습니다.');
    }

    final items = channel
        .findElements('item')
        .take(count.clamp(1, 6))
        .map(_normalizeFeedItem)
        .toList();
    if (items.isEmpty) {
      throw Exception('표시할 뉴스 항목이 없습니다.');
    }

    final lead = items.first;
    final secondary = items.skip(1).take(5).toList();
    final breakingCount = items.where((item) => item.isBreaking).length;
    final updatedAt = _formatUpdatedAt(
      _parseRfc822(_childText(channel, 'lastBuildDate')),
    );

    final model = <String, Object?>{
      'title': '오늘의 주요 뉴스',
      'headline': lead.title,
      'updatedAt': updatedAt,
      'publisherValue': '연합뉴스TV',
      'publisherDetail': updatedAt,
      'headlineMetricValue': '${items.length}건',
      'headlineMetricDetail': '최신 RSS 피드',
      'breakingMetricValue': '$breakingCount건',
      'breakingMetricDetail': '제목 기준 속보',
      'leadLabel': lead.label,
      'leadValue': lead.title,
      'leadDetail': lead.detail,
      'headlineSectionTitle': '최신 헤드라인',
      'secondaryCount': secondary.length,
      'footerTitle': '데이터 메모',
      'footer': '연합뉴스TV RSS 기준 · 헤드라인과 갱신 시각을 함께 표시합니다.',
    };

    for (var index = 0; index < secondary.length; index += 1) {
      final item = secondary[index];
      final slot = index + 1;
      model['headline${slot}Label'] = item.label;
      model['headline${slot}Value'] = item.title;
      model['headline${slot}Detail'] = item.detail;
    }

    return model;
  }

  String _defaultFeedUrl() {
    if (kIsWeb) {
      return webProxyUrl;
    }
    return 'https://www.yonhapnewstv.co.kr/browse/feed/';
  }
}

class NewsTemplatePresenter implements TemplateSurfacePresenter {
  const NewsTemplatePresenter({required this.repository, this.count = 6});

  final NewsBriefingRepository repository;
  final int count;

  @override
  TemplateSurfaceStyle get surfaceStyle => TemplateSurfaceStyle.newsPanel;

  @override
  TemplateSurfacePayload? buildLoading() {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '뉴스 준비 중',
        detail: '주요 헤드라인과 카테고리별 뉴스를 정리하고 있습니다.',
        hint: '리드 스토리부터 표시한 뒤 나머지 헤드라인을 채웁니다.',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  TemplateSurfacePayload buildError(Object error) {
    final detail = kIsWeb
        ? '로컬 뉴스 프록시 또는 피드 연결 상태를 확인해 주세요.'
        : '피드 연결 또는 요약 파이프라인 상태를 확인해 주세요.';
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '뉴스를 불러오지 못했습니다',
        detail: detail,
        hint: _cleanText('$error', maxLen: 100),
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    final dataModel = await repository.fetchNewsBriefing(count: count);
    final secondaryCount = (dataModel['secondaryCount'] as int?) ?? 0;
    return TemplateSurfacePayload(
      components: buildNewsBriefingComponents(headlineCount: secondaryCount),
      dataModel: dataModel,
      surfaceStyle: surfaceStyle,
    );
  }
}

List<Component> buildNewsBriefingComponents({int headlineCount = 5}) {
  final headlineChildren = <String>[
    'headlineHeaderRow',
    'headlineHeaderDivider',
  ];
  final components = <Component>[
    _component('root', 'Column', {
      'alignment': 'stretch',
      'children': ['heroCard', 'metricRow', 'headlineCard', 'footerCard'],
    }),
    _component('heroCard', 'Card', {'child': 'heroColumn'}),
    _component('heroColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['heroTopRow', 'headlineText', 'heroDivider', 'leadMetaRow'],
    }),
    _component('heroTopRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['titleText', 'updatedColumn'],
    }),
    _textPath('titleText', '/title', usageHint: 'h3', weight: 3),
    _component('updatedColumn', 'Column', {
      'alignment': 'end',
      'children': ['updatedLabel', 'updatedText'],
    }, weight: 2),
    _textLiteral('updatedLabel', '업데이트', usageHint: 'caption'),
    _textPath('updatedText', '/updatedAt', usageHint: 'caption'),
    _textPath('headlineText', '/headline', usageHint: 'h2'),
    _component('heroDivider', 'Divider', const {}),
    _component('leadMetaRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['leadLabelColumn', 'leadDetailColumn'],
    }),
    _component('leadLabelColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['leadSectionLabel', 'leadLabelText'],
    }, weight: 2),
    _textLiteral('leadSectionLabel', '분류', usageHint: 'caption'),
    _textPath('leadLabelText', '/leadLabel', usageHint: 'h5'),
    _component('leadDetailColumn', 'Column', {
      'alignment': 'end',
      'children': ['leadDetailLabel', 'leadDetailText'],
    }, weight: 3),
    _textLiteral('leadDetailLabel', '게시 시각', usageHint: 'caption'),
    _textPath('leadDetailText', '/leadDetail', usageHint: 'caption'),
    _component('metricRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['publisherCard', 'headlineMetricCard', 'breakingMetricCard'],
    }),
    _component('publisherCard', 'Card', {
      'child': 'publisherColumn',
    }, weight: 1),
    _component('publisherColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'publisherLabel',
        'publisherValueText',
        'publisherDetailText',
      ],
    }),
    _textLiteral('publisherLabel', '출처', usageHint: 'caption'),
    _textPath('publisherValueText', '/publisherValue', usageHint: 'h4'),
    _textPath('publisherDetailText', '/publisherDetail', usageHint: 'caption'),
    _component('headlineMetricCard', 'Card', {
      'child': 'headlineMetricColumn',
    }, weight: 1),
    _component('headlineMetricColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'headlineMetricLabel',
        'headlineMetricValueText',
        'headlineMetricDetailText',
      ],
    }),
    _textLiteral('headlineMetricLabel', '헤드라인', usageHint: 'caption'),
    _textPath(
      'headlineMetricValueText',
      '/headlineMetricValue',
      usageHint: 'h4',
    ),
    _textPath(
      'headlineMetricDetailText',
      '/headlineMetricDetail',
      usageHint: 'caption',
    ),
    _component('breakingMetricCard', 'Card', {
      'child': 'breakingMetricColumn',
    }, weight: 1),
    _component('breakingMetricColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'breakingMetricLabel',
        'breakingMetricValueText',
        'breakingMetricDetailText',
      ],
    }),
    _textLiteral('breakingMetricLabel', '속보', usageHint: 'caption'),
    _textPath(
      'breakingMetricValueText',
      '/breakingMetricValue',
      usageHint: 'h4',
    ),
    _textPath(
      'breakingMetricDetailText',
      '/breakingMetricDetail',
      usageHint: 'caption',
    ),
    _component('headlineCard', 'Card', {'child': 'headlineColumn'}),
    _component('headlineColumn', 'Column', {
      'alignment': 'stretch',
      'children': headlineChildren,
    }),
    _component('headlineHeaderRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'center',
      'children': ['headlineSectionTitleText', 'headlineCountText'],
    }),
    _textPath(
      'headlineSectionTitleText',
      '/headlineSectionTitle',
      usageHint: 'h4',
    ),
    _textPath(
      'headlineCountText',
      '/headlineMetricValue',
      usageHint: 'caption',
    ),
    _component('headlineHeaderDivider', 'Divider', const {}),
    _component('footerCard', 'Card', {'child': 'footerRow'}),
    _component('footerRow', 'Row', {
      'distribution': 'start',
      'alignment': 'start',
      'children': ['footerIcon', 'footerColumn'],
    }),
    _component('footerIcon', 'Icon', {
      'name': {'literalString': 'info'},
    }),
    _component('footerColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['footerTitleText', 'footerText'],
    }, weight: 1),
    _textPath('footerTitleText', '/footerTitle', usageHint: 'caption'),
    _textPath('footerText', '/footer', usageHint: 'caption'),
  ];

  for (var index = 1; index <= headlineCount.clamp(0, 5); index += 1) {
    final itemId = 'headline${index}ItemColumn';
    final topRowId = 'headline${index}TopRow';
    final dividerId = 'headline${index}Divider';
    final labelId = 'headline${index}LabelText';
    final valueId = 'headline${index}ValueText';
    final detailId = 'headline${index}DetailText';

    components.addAll([
      _component(itemId, 'Column', {
        'alignment': 'stretch',
        'children': [topRowId, valueId],
      }),
      _component(topRowId, 'Row', {
        'distribution': 'spaceBetween',
        'alignment': 'center',
        'children': [labelId, detailId],
      }),
      _textPath(labelId, '/headline${index}Label', usageHint: 'caption'),
      _textPath(valueId, '/headline${index}Value', usageHint: 'body'),
      _textPath(detailId, '/headline${index}Detail', usageHint: 'caption'),
    ]);
    headlineChildren.add(itemId);
    if (index < headlineCount.clamp(0, 5)) {
      components.add(_component(dividerId, 'Divider', const {}));
      headlineChildren.add(dividerId);
    }
  }

  return components;
}

class _NewsFeedItem {
  const _NewsFeedItem({
    required this.title,
    required this.label,
    required this.detail,
    required this.isBreaking,
  });

  final String title;
  final String label;
  final String detail;
  final bool isBreaking;
}

_NewsFeedItem _normalizeFeedItem(XmlElement item) {
  final title = _cleanText(_childText(item, 'title'), maxLen: 88);
  final category = _cleanText(_childText(item, 'category'), maxLen: 20);
  final publishedAt = _parseRfc822(_childText(item, 'pubDate'));
  final label = title.contains('[속보]')
      ? '속보'
      : (category.isEmpty ? '최신' : category);
  final detail = publishedAt == null
      ? '시각 확인 필요 · 연합뉴스TV'
      : '${_formatPublishedAt(publishedAt)} · 연합뉴스TV';

  return _NewsFeedItem(
    title: title,
    label: label,
    detail: detail,
    isBreaking: title.contains('[속보]'),
  );
}

String _childText(XmlElement parent, String name) {
  return parent.getElement(name)?.innerText.trim() ?? '';
}

T? _firstOrNull<T>(Iterable<T> values) {
  for (final value in values) {
    return value;
  }
  return null;
}

DateTime? _parseRfc822(String value) {
  if (value.isEmpty) {
    return null;
  }

  final normalized = value.replaceFirst(RegExp(r'^[A-Za-z]{3},\s*'), '');
  final parts = normalized.split(RegExp(r'\s+'));
  if (parts.length < 5) {
    return null;
  }

  final day = int.tryParse(parts[0]);
  final month = _monthIndex(parts[1]);
  final year = int.tryParse(parts[2]);
  final time = parts[3].split(':');
  final offset = parts[4];
  if (day == null || month == null || year == null || time.length < 2) {
    return null;
  }

  final hour = int.tryParse(time[0]);
  final minute = int.tryParse(time[1]);
  final second = time.length > 2 ? int.tryParse(time[2]) ?? 0 : 0;
  if (hour == null || minute == null) {
    return null;
  }

  final dateTime = DateTime.utc(year, month, day, hour, minute, second);
  final sign = offset.startsWith('-') ? -1 : 1;
  final cleanOffset = offset.replaceAll(RegExp(r'[^0-9]'), '');
  if (cleanOffset.length != 4) {
    return dateTime;
  }
  final offsetHours = int.tryParse(cleanOffset.substring(0, 2)) ?? 0;
  final offsetMinutes = int.tryParse(cleanOffset.substring(2, 4)) ?? 0;
  final delta = Duration(hours: offsetHours, minutes: offsetMinutes);
  return dateTime.subtract(sign == 1 ? delta : -delta).toLocal();
}

int? _monthIndex(String month) {
  const months = {
    'Jan': 1,
    'Feb': 2,
    'Mar': 3,
    'Apr': 4,
    'May': 5,
    'Jun': 6,
    'Jul': 7,
    'Aug': 8,
    'Sep': 9,
    'Oct': 10,
    'Nov': 11,
    'Dec': 12,
  };
  return months[month];
}

String _formatUpdatedAt(DateTime? dateTime) {
  if (dateTime == null) {
    return '갱신 시각 확인 필요';
  }
  final localTime = dateTime.toLocal();
  final meridiem = localTime.hour < 12 ? '오전' : '오후';
  final hour = localTime.hour % 12 == 0 ? 12 : localTime.hour % 12;
  return '$meridiem $hour:${localTime.minute.toString().padLeft(2, '0')} 기준';
}

String _formatPublishedAt(DateTime dateTime) {
  final localTime = dateTime.toLocal();
  final meridiem = localTime.hour < 12 ? '오전' : '오후';
  final hour = localTime.hour % 12 == 0 ? 12 : localTime.hour % 12;
  return '$meridiem $hour:${localTime.minute.toString().padLeft(2, '0')}';
}

Component _component(
  String id,
  String type,
  Map<String, Object?> properties, {
  int? weight,
}) {
  return Component(
    id: id,
    componentProperties: {type: properties},
    weight: weight,
  );
}

Component _textPath(
  String id,
  String path, {
  String usageHint = 'body',
  int? weight,
}) {
  return Component(
    id: id,
    componentProperties: {
      'Text': {
        'text': {'path': path},
        'usageHint': usageHint,
      },
    },
    weight: weight,
  );
}

Component _textLiteral(
  String id,
  String text, {
  String usageHint = 'body',
  int? weight,
}) {
  return Component(
    id: id,
    componentProperties: {
      'Text': {
        'text': {'literalString': text},
        'usageHint': usageHint,
      },
    },
    weight: weight,
  );
}

String _cleanText(Object? value, {int maxLen = 80}) {
  final text = '${value ?? ''}'.trim().replaceAll(RegExp(r'\s+'), ' ');
  if (text.length <= maxLen) {
    return text;
  }
  return '${text.substring(0, maxLen - 3).trimRight()}...';
}
