import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:genui/genui.dart';
import 'package:http/http.dart' as http;

import '../../home/models/template_surface.dart';

abstract interface class ScheduleBriefingRepository {
  Future<Map<String, Object?>> fetchScheduleBriefing({
    int windowDays = 120,
    int maxEvents = 6,
    DateTime? now,
  });
}

class IcsScheduleBriefingRepository implements ScheduleBriefingRepository {
  const IcsScheduleBriefingRepository({
    this.icsUrl,
    this.webProxyUrl = 'http://127.0.0.1:3001/schedule-feed',
  });

  final String? icsUrl;
  final String webProxyUrl;

  @override
  Future<Map<String, Object?>> fetchScheduleBriefing({
    int windowDays = 120,
    int maxEvents = 6,
    DateTime? now,
  }) async {
    final targetUrl = icsUrl ?? _defaultFeedUrl();
    final response = await http
        .get(Uri.parse(targetUrl))
        .timeout(const Duration(seconds: 6));
    if (response.statusCode != 200) {
      throw Exception('일정 ICS 응답 실패 (${response.statusCode})');
    }

    final referenceNow = now ?? DateTime.now();
    final events = _parseIcsEvents(
      utf8.decode(response.bodyBytes),
      now: referenceNow,
      windowDays: windowDays,
      maxEvents: maxEvents,
    );
    if (events.isEmpty) {
      throw const _NoScheduleEventsException();
    }

    final currentEvents = events
        .where(
          (event) =>
              event.start.isBefore(referenceNow) &&
              event.end.isAfter(referenceNow),
        )
        .toList();
    final futureEvents = events
        .where((event) => event.start.isAfter(referenceNow))
        .toList();
    final nextEvent = futureEvents.isNotEmpty
        ? futureEvents.first
        : events.first;
    final currentEvent = currentEvents.isNotEmpty ? currentEvents.first : null;
    final todayEnd = DateTime(
      referenceNow.year,
      referenceNow.month,
      referenceNow.day,
      23,
      59,
      59,
    );
    final todayEvents = events
        .where((event) => !event.start.isAfter(todayEnd))
        .toList();
    final listEvents = (todayEvents.isNotEmpty ? todayEvents : events)
        .take(4)
        .toList();

    final model = <String, Object?>{
      'title': '오늘 일정 브리핑',
      'headline': _buildHeadline(
        currentEvent: currentEvent,
        nextEvent: nextEvent,
      ),
      'updatedAt': _formatUpdatedAt(referenceNow),
      'nextMetricValue': _eventValue(nextEvent),
      'nextMetricDetail': _eventDetail(nextEvent),
      'todayMetricValue': '${todayEvents.length}건',
      'todayMetricDetail': _formatKoreanDate(referenceNow),
      'currentMetricValue': currentEvent?.summary ?? '없음',
      'currentMetricDetail': currentEvent == null
          ? '현재 진행 중인 일정 없음'
          : _eventDetail(currentEvent),
      'focusTitle': '지금과 다음',
      'currentLabel': '현재',
      'currentValue': currentEvent?.summary ?? '진행 중 일정 없음',
      'currentDetail': currentEvent == null
          ? '다음 일정만 준비하면 됩니다.'
          : _eventDetail(currentEvent),
      'nextLabel': '다음',
      'nextValue': nextEvent.summary,
      'nextDetail': _eventDetail(nextEvent),
      'eventsTitle': todayEvents.isNotEmpty ? '오늘 일정' : '다가오는 일정',
      'eventsCountText': '${listEvents.length}건',
      'noticeTitle': '공용 화면 주의',
      'noticeSummary':
          '참석자 이름, 상세 메모, 정확한 위치는 기본적으로 축약하거나 감춘 형태로 표시하는 편이 안전합니다.',
      'noticeMeta': 'ICS 일정 요약',
      'footerTitle': '데이터 메모',
      'footerText': '대한민국 공휴일 ICS 또는 연결된 캘린더 ICS를 한국 시각 기준으로 요약합니다.',
    };

    for (var index = 0; index < listEvents.length; index += 1) {
      final event = listEvents[index];
      final slot = index + 1;
      model['event${slot}Label'] = event.summary;
      model['event${slot}Value'] = event.allDay
          ? _formatKoreanDate(event.start)
          : _formatKoreanDateTime(event.start);
      model['event${slot}Detail'] = _eventDetail(event);
    }

    return model;
  }

  String _defaultFeedUrl() {
    if (kIsWeb) {
      return webProxyUrl;
    }
    return 'https://holidays.hyunbin.page/basic.ics';
  }
}

class ScheduleTemplatePresenter implements TemplateSurfacePresenter {
  const ScheduleTemplatePresenter({
    required this.repository,
    this.windowDays = 120,
    this.maxEvents = 6,
  });

  final ScheduleBriefingRepository repository;
  final int windowDays;
  final int maxEvents;

  @override
  TemplateSurfaceStyle get surfaceStyle => TemplateSurfaceStyle.schedulePanel;

  @override
  TemplateSurfacePayload? buildLoading() {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '일정 준비 중',
        detail: '오늘 일정과 다음 약속을 정리하고 있습니다.',
        hint: '현재 일정, 다음 일정, 오늘 남은 일정을 먼저 채웁니다.',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  TemplateSurfacePayload buildError(Object error) {
    if (error is _NoScheduleEventsException) {
      return TemplateSurfacePayload(
        components: buildTemplateStatusComponents(),
        dataModel: buildTemplateStatusModel(
          title: '표시할 일정 없음',
          detail: '현재 선택한 창에서는 일정이 없습니다.',
          hint: '캘린더 연동 또는 일정 조회 범위를 다시 확인해 주세요.',
        ),
        surfaceStyle: surfaceStyle,
      );
    }

    final detail = kIsWeb
        ? '로컬 일정 프록시 또는 ICS 피드 연결 상태를 확인해 주세요.'
        : 'ICS URL 또는 파일 연결 상태를 확인해 주세요.';
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '일정을 불러오지 못했습니다',
        detail: detail,
        hint: _cleanText('$error', maxLen: 100),
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    final dataModel = await repository.fetchScheduleBriefing(
      windowDays: windowDays,
      maxEvents: maxEvents,
    );
    final eventCount = <int>[
      1,
      2,
      3,
      4,
    ].where((index) => dataModel['event${index}Label'] != null).length;
    return TemplateSurfacePayload(
      components: buildScheduleBriefingComponents(eventCount: eventCount),
      dataModel: dataModel,
      surfaceStyle: surfaceStyle,
    );
  }
}

List<Component> buildScheduleBriefingComponents({int eventCount = 4}) {
  final eventChildren = <String>['eventsHeaderRow', 'eventsHeaderDivider'];
  final components = <Component>[
    _component('root', 'Column', {
      'alignment': 'stretch',
      'children': [
        'heroCard',
        'metricRow',
        'focusCard',
        'eventsCard',
        'noticeCard',
      ],
    }),
    _component('heroCard', 'Card', {'child': 'heroColumn'}),
    _component('heroColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['heroTopRow', 'headlineText'],
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
    _textPath('headlineText', '/headline', usageHint: 'h5'),
    _component('metricRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['nextMetricCard', 'todayMetricCard', 'currentMetricCard'],
    }),
    _component('nextMetricCard', 'Card', {
      'child': 'nextMetricColumn',
    }, weight: 1),
    _component('nextMetricColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'nextMetricLabel',
        'nextMetricValueText',
        'nextMetricDetailText',
      ],
    }),
    _textLiteral('nextMetricLabel', '다음 일정', usageHint: 'caption'),
    _textPath('nextMetricValueText', '/nextMetricValue', usageHint: 'h4'),
    _textPath(
      'nextMetricDetailText',
      '/nextMetricDetail',
      usageHint: 'caption',
    ),
    _component('todayMetricCard', 'Card', {
      'child': 'todayMetricColumn',
    }, weight: 1),
    _component('todayMetricColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'todayMetricLabel',
        'todayMetricValueText',
        'todayMetricDetailText',
      ],
    }),
    _textLiteral('todayMetricLabel', '오늘 남은 일정', usageHint: 'caption'),
    _textPath('todayMetricValueText', '/todayMetricValue', usageHint: 'h4'),
    _textPath(
      'todayMetricDetailText',
      '/todayMetricDetail',
      usageHint: 'caption',
    ),
    _component('currentMetricCard', 'Card', {
      'child': 'currentMetricColumn',
    }, weight: 1),
    _component('currentMetricColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'currentMetricLabel',
        'currentMetricValueText',
        'currentMetricDetailText',
      ],
    }),
    _textLiteral('currentMetricLabel', '현재 진행', usageHint: 'caption'),
    _textPath('currentMetricValueText', '/currentMetricValue', usageHint: 'h4'),
    _textPath(
      'currentMetricDetailText',
      '/currentMetricDetail',
      usageHint: 'caption',
    ),
    _component('focusCard', 'Card', {'child': 'focusColumn'}),
    _component('focusColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['focusTitleText', 'focusDivider', 'focusRow'],
    }),
    _textPath('focusTitleText', '/focusTitle', usageHint: 'h4'),
    _component('focusDivider', 'Divider', const {}),
    _component('focusRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['currentFocusColumn', 'nextFocusColumn'],
    }),
    _component('currentFocusColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['currentLabelText', 'currentValueText', 'currentDetailText'],
    }, weight: 1),
    _textPath('currentLabelText', '/currentLabel', usageHint: 'caption'),
    _textPath('currentValueText', '/currentValue', usageHint: 'h5'),
    _textPath('currentDetailText', '/currentDetail', usageHint: 'caption'),
    _component('nextFocusColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['nextLabelText', 'nextValueText', 'nextDetailText'],
    }, weight: 1),
    _textPath('nextLabelText', '/nextLabel', usageHint: 'caption'),
    _textPath('nextValueText', '/nextValue', usageHint: 'h5'),
    _textPath('nextDetailText', '/nextDetail', usageHint: 'caption'),
    _component('eventsCard', 'Card', {'child': 'eventsColumn'}),
    _component('eventsColumn', 'Column', {
      'alignment': 'stretch',
      'children': eventChildren,
    }),
    _component('eventsHeaderRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'center',
      'children': ['eventsTitleText', 'eventsCountText'],
    }),
    _textPath('eventsTitleText', '/eventsTitle', usageHint: 'h4'),
    _textPath('eventsCountText', '/eventsCountText', usageHint: 'caption'),
    _component('eventsHeaderDivider', 'Divider', const {}),
    _component('noticeCard', 'Card', {'child': 'noticeRow'}),
    _component('noticeRow', 'Row', {
      'distribution': 'start',
      'alignment': 'start',
      'children': ['noticeIcon', 'noticeColumn'],
    }),
    _component('noticeIcon', 'Icon', {
      'name': {'literalString': 'visibilityOff'},
    }),
    _component('noticeColumn', 'Column', {
      'alignment': 'stretch',
      'children': [
        'noticeTitleText',
        'noticeSummaryText',
        'noticeMetaText',
        'footerTitleText',
        'footerText',
      ],
    }, weight: 1),
    _textPath('noticeTitleText', '/noticeTitle', usageHint: 'caption'),
    _textPath('noticeSummaryText', '/noticeSummary'),
    _textPath('noticeMetaText', '/noticeMeta', usageHint: 'caption'),
    _textPath('footerTitleText', '/footerTitle', usageHint: 'caption'),
    _textPath('footerText', '/footerText', usageHint: 'caption'),
  ];

  for (var index = 1; index <= eventCount.clamp(0, 4); index += 1) {
    final itemId = 'event${index}Column';
    final topRowId = 'event${index}TopRow';
    final labelId = 'event${index}LabelText';
    final valueId = 'event${index}ValueText';
    final detailId = 'event${index}DetailText';
    final dividerId = 'event${index}Divider';

    components.addAll([
      _component(itemId, 'Column', {
        'alignment': 'stretch',
        'children': [topRowId, detailId],
      }),
      _component(topRowId, 'Row', {
        'distribution': 'spaceBetween',
        'alignment': 'center',
        'children': [labelId, valueId],
      }),
      _textPath(labelId, '/event${index}Label', usageHint: 'body'),
      _textPath(valueId, '/event${index}Value', usageHint: 'caption'),
      _textPath(detailId, '/event${index}Detail', usageHint: 'caption'),
    ]);
    eventChildren.add(itemId);
    if (index < eventCount.clamp(0, 4)) {
      components.add(_component(dividerId, 'Divider', const {}));
      eventChildren.add(dividerId);
    }
  }

  return components;
}

class _IcsEvent {
  const _IcsEvent({
    required this.summary,
    required this.start,
    required this.end,
    required this.allDay,
    this.location,
    this.description,
  });

  final String summary;
  final DateTime start;
  final DateTime end;
  final bool allDay;
  final String? location;
  final String? description;
}

class _IcsProperty {
  const _IcsProperty({
    required this.name,
    required this.params,
    required this.value,
  });

  final String name;
  final List<String> params;
  final String value;
}

class _NoScheduleEventsException implements Exception {
  const _NoScheduleEventsException();
}

List<_IcsEvent> _parseIcsEvents(
  String text, {
  required DateTime now,
  required int windowDays,
  required int maxEvents,
}) {
  final lines = _unfoldLines(text);
  final events = <_IcsEvent>[];
  final windowEnd = now.add(Duration(days: windowDays.clamp(1, 365)));
  var inEvent = false;
  final properties = <_IcsProperty>[];

  for (final line in lines) {
    if (line == 'BEGIN:VEVENT') {
      inEvent = true;
      properties.clear();
      continue;
    }
    if (line == 'END:VEVENT') {
      inEvent = false;
      final event = _buildEvent(properties);
      if (event != null &&
          event.end.isAfter(now) &&
          !event.start.isAfter(windowEnd)) {
        events.add(event);
      }
      properties.clear();
      continue;
    }
    if (!inEvent || !line.contains(':')) {
      continue;
    }

    final separator = line.indexOf(':');
    final namePart = line.substring(0, separator);
    final value = line.substring(separator + 1);
    final segments = namePart.split(';');
    properties.add(
      _IcsProperty(
        name: segments.first.toUpperCase(),
        params: segments
            .skip(1)
            .map((segment) => segment.toUpperCase())
            .toList(),
        value: value,
      ),
    );
  }

  events.sort((left, right) => left.start.compareTo(right.start));
  return events.take(maxEvents.clamp(1, 12)).toList();
}

List<String> _unfoldLines(String text) {
  final normalized = text.replaceAll('\r\n', '\n').replaceAll('\r', '\n');
  final unfolded = <String>[];
  for (final line in normalized.split('\n')) {
    if ((line.startsWith(' ') || line.startsWith('\t')) &&
        unfolded.isNotEmpty) {
      unfolded[unfolded.length - 1] += line.substring(1);
      continue;
    }
    unfolded.add(line.trimRight());
  }
  return unfolded;
}

_IcsEvent? _buildEvent(List<_IcsProperty> properties) {
  _IcsProperty? find(String name) {
    for (final property in properties) {
      if (property.name == name) {
        return property;
      }
    }
    return null;
  }

  final startProperty = find('DTSTART');
  final summaryProperty = find('SUMMARY');
  if (startProperty == null || summaryProperty == null) {
    return null;
  }

  final allDay =
      startProperty.params.any((param) => param == 'VALUE=DATE') ||
      RegExp(r'^\d{8}$').hasMatch(startProperty.value);
  final start = _parseIcsDateTime(startProperty.value, allDay: allDay);
  if (start == null) {
    return null;
  }

  final endProperty = find('DTEND');
  final end = endProperty == null
      ? start.add(allDay ? const Duration(days: 1) : const Duration(hours: 1))
      : _parseIcsDateTime(
              endProperty.value,
              allDay:
                  endProperty.params.any((param) => param == 'VALUE=DATE') ||
                  allDay,
            ) ??
            start.add(
              allDay ? const Duration(days: 1) : const Duration(hours: 1),
            );

  final summary = _cleanText(_decodeIcsText(summaryProperty.value), maxLen: 44);
  if (summary.isEmpty) {
    return null;
  }

  final location = _cleanText(
    _decodeIcsText(find('LOCATION')?.value),
    maxLen: 28,
  );
  final description = _cleanText(
    _decodeIcsText(find('DESCRIPTION')?.value),
    maxLen: 40,
  );

  return _IcsEvent(
    summary: summary,
    start: start,
    end: end.isAfter(start)
        ? end
        : start.add(
            allDay ? const Duration(days: 1) : const Duration(hours: 1),
          ),
    allDay: allDay,
    location: location.isEmpty ? null : location,
    description: description.isEmpty ? null : description,
  );
}

DateTime? _parseIcsDateTime(String value, {required bool allDay}) {
  if (allDay) {
    final match = RegExp(r'^(\d{4})(\d{2})(\d{2})$').firstMatch(value);
    if (match == null) {
      return null;
    }
    return DateTime(
      int.parse(match.group(1)!),
      int.parse(match.group(2)!),
      int.parse(match.group(3)!),
    );
  }

  final match = RegExp(
    r'^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})?(Z)?$',
  ).firstMatch(value);
  if (match == null) {
    return null;
  }

  final year = int.parse(match.group(1)!);
  final month = int.parse(match.group(2)!);
  final day = int.parse(match.group(3)!);
  final hour = int.parse(match.group(4)!);
  final minute = int.parse(match.group(5)!);
  final second = int.tryParse(match.group(6) ?? '0') ?? 0;
  final isUtc = match.group(7) == 'Z';
  if (isUtc) {
    return DateTime.utc(year, month, day, hour, minute, second).toLocal();
  }
  return DateTime(year, month, day, hour, minute, second);
}

String _buildHeadline({
  required _IcsEvent? currentEvent,
  required _IcsEvent nextEvent,
}) {
  if (currentEvent != null) {
    return _cleanText(
      '지금은 ${currentEvent.summary} 일정이 진행 중이며, 다음 일정은 ${nextEvent.summary}입니다.',
      maxLen: 88,
    );
  }
  return _cleanText('다음 일정은 ${_eventValue(nextEvent)}입니다.', maxLen: 88);
}

String _eventValue(_IcsEvent event) {
  if (event.allDay) {
    return _cleanText('${event.summary} · 종일', maxLen: 36);
  }
  return _cleanText(
    '${_formatKoreanDateTime(event.start)} ${event.summary}',
    maxLen: 36,
  );
}

String _eventDetail(_IcsEvent event) {
  final parts = <String>[_formatKoreanDate(event.start)];
  if (event.allDay) {
    parts.add('종일');
  } else {
    parts.add(
      '${_formatKoreanTime(event.start)}-${_formatKoreanTime(event.end)}',
    );
  }
  if (event.location != null && event.location!.isNotEmpty) {
    parts.add(event.location!);
  } else if (event.description != null && event.description!.isNotEmpty) {
    parts.add(event.description!);
  }
  return _cleanText(parts.join(' · '), maxLen: 72);
}

String _formatUpdatedAt(DateTime dateTime) {
  return '${_formatKoreanTime(dateTime)} 기준';
}

String _formatKoreanDate(DateTime dateTime) {
  return '${dateTime.month}월 ${dateTime.day}일';
}

String _formatKoreanDateTime(DateTime dateTime) {
  return '${_formatKoreanDate(dateTime)} ${_formatKoreanTime(dateTime)}';
}

String _formatKoreanTime(DateTime dateTime) {
  final meridiem = dateTime.hour < 12 ? '오전' : '오후';
  final hour = dateTime.hour % 12 == 0 ? 12 : dateTime.hour % 12;
  return '$meridiem $hour:${dateTime.minute.toString().padLeft(2, '0')}';
}

String _decodeIcsText(String? value) {
  if (value == null || value.isEmpty) {
    return '';
  }
  return value
      .replaceAll(r'\n', ' ')
      .replaceAll(r'\N', ' ')
      .replaceAll(r'\,', ',')
      .replaceAll(r'\;', ';')
      .replaceAll(r'\\', r'\')
      .trim();
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
