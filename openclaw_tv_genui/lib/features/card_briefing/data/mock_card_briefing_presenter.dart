import 'dart:convert';

import 'package:http/http.dart' as http;
import 'package:genui/genui.dart';

import '../../home/models/template_surface.dart';

abstract interface class CardBriefingRepository {
  Future<Map<String, Object?>> fetchBriefing();
}

class HttpCardBriefingRepository implements CardBriefingRepository {
  const HttpCardBriefingRepository({
    required this.endpointUrl,
    this.timeout = const Duration(seconds: 8),
  });

  final String endpointUrl;
  final Duration timeout;

  @override
  Future<Map<String, Object?>> fetchBriefing() async {
    final response = await http.get(Uri.parse(endpointUrl)).timeout(timeout);
    if (response.statusCode != 200) {
      throw Exception('브리핑 응답 실패 (${response.statusCode})');
    }

    final decoded = jsonDecode(utf8.decode(response.bodyBytes));
    if (decoded is! Map) {
      throw Exception('브리핑 응답 형식이 올바르지 않습니다.');
    }
    return decoded.cast<String, Object?>();
  }
}

class MockCardBriefingPresenter implements TemplateSurfacePresenter {
  const MockCardBriefingPresenter({
    required this.title,
    required this.contract,
    this.surfaceStyle = TemplateSurfaceStyle.schedulePanel,
  });

  final String title;
  final Map<String, Object?> contract;

  @override
  final TemplateSurfaceStyle surfaceStyle;

  @override
  TemplateSurfacePayload? buildLoading() => null;

  @override
  TemplateSurfacePayload buildError(Object error) {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '$title 오류',
        detail: '카드형 브리핑을 렌더링하지 못했습니다.',
        hint: '$error',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    return buildCardBriefingPayload(contract, surfaceStyle: surfaceStyle);
  }
}

class LiveCardBriefingPresenter implements TemplateSurfacePresenter {
  const LiveCardBriefingPresenter({
    required this.title,
    required this.repository,
    required this.loadingTitle,
    required this.loadingDetail,
    required this.loadingHint,
    required this.errorTitle,
    required this.errorDetail,
    this.surfaceStyle = TemplateSurfaceStyle.schedulePanel,
  });

  final String title;
  final CardBriefingRepository repository;
  final String loadingTitle;
  final String loadingDetail;
  final String loadingHint;
  final String errorTitle;
  final String errorDetail;

  @override
  final TemplateSurfaceStyle surfaceStyle;

  @override
  TemplateSurfacePayload? buildLoading() {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: loadingTitle,
        detail: loadingDetail,
        hint: loadingHint,
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  TemplateSurfacePayload buildError(Object error) {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: errorTitle,
        detail: errorDetail,
        hint: _cleanText('$error', maxLen: 100),
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    final contract = await repository.fetchBriefing();
    return buildCardBriefingPayload(contract, surfaceStyle: surfaceStyle);
  }
}

TemplateSurfacePayload buildCardBriefingPayload(
  Map<String, Object?> contract, {
  required TemplateSurfaceStyle surfaceStyle,
}) {
  final normalized = normalizeCardBriefingContract(contract);
  final sectionCounts = extractCardBriefingSectionCounts(normalized);

  return TemplateSurfacePayload(
    components: buildCardBriefingComponents(sectionItemCounts: sectionCounts),
    dataModel: normalized,
    surfaceStyle: surfaceStyle,
  );
}

List<int> extractCardBriefingSectionCounts(Map<String, Object?> normalized) {
  final sectionCounts = <int>[];

  for (var sectionIndex = 1; ; sectionIndex += 1) {
    if (!normalized.containsKey('section${sectionIndex}Title')) {
      break;
    }

    final count = (normalized['section${sectionIndex}ItemCount'] as int?) ?? 0;
    if (count > 0) {
      sectionCounts.add(count);
    }
  }

  return sectionCounts;
}

Map<String, Object?> normalizeCardBriefingContract(
  Map<String, Object?> contract,
) {
  final model = <String, Object?>{
    'title': _cleanText(contract['title'], maxLen: 32),
    'headline': _cleanText(contract['headline'], maxLen: 88),
    'alertTitle': '',
    'alertSummary': '',
    'alertMeta': '',
    'footer': _cleanText(contract['footer'], maxLen: 120),
  };

  final metrics =
      (contract['primaryMetrics'] as List?)?.cast<Map<String, Object?>>() ??
      const <Map<String, Object?>>[];
  for (var index = 0; index < metrics.length && index < 3; index += 1) {
    final slot = index + 1;
    final metric = metrics[index];
    model['metric${slot}Label'] = _cleanText(metric['label'], maxLen: 18);
    model['metric${slot}Value'] = _cleanText(metric['value'], maxLen: 32);
    model['metric${slot}Detail'] = _cleanText(metric['detail'], maxLen: 56);
  }

  final sections =
      (contract['sections'] as List?)?.cast<Map<String, Object?>>() ??
      const <Map<String, Object?>>[];
  for (
    var sectionIndex = 0;
    sectionIndex < sections.length;
    sectionIndex += 1
  ) {
    final sectionSlot = sectionIndex + 1;
    final section = sections[sectionIndex];
    final items =
        (section['items'] as List?)?.cast<Map<String, Object?>>() ??
        const <Map<String, Object?>>[];
    model['section${sectionSlot}Title'] = _cleanText(
      section['title'],
      maxLen: 28,
    );
    model['section${sectionSlot}CountText'] = '${items.length}개';
    model['section${sectionSlot}ItemCount'] = items.length.clamp(0, 4);

    for (
      var itemIndex = 0;
      itemIndex < items.length && itemIndex < 4;
      itemIndex += 1
    ) {
      final itemSlot = itemIndex + 1;
      final item = items[itemIndex];
      model['section${sectionSlot}Item${itemSlot}Label'] = _cleanText(
        item['label'],
        maxLen: 24,
      );
      model['section${sectionSlot}Item${itemSlot}Value'] = _cleanText(
        item['value'],
        maxLen: 40,
      );
      model['section${sectionSlot}Item${itemSlot}Detail'] = _cleanText(
        item['detail'],
        maxLen: 72,
      );
    }
  }

  final alert = (contract['alert'] as Map?)?.cast<String, Object?>();
  if (alert != null) {
    model['alertTitle'] = _cleanText(alert['title'], maxLen: 24);
    model['alertSummary'] = _cleanText(alert['summary'], maxLen: 120);
    model['alertMeta'] = _cleanText(alert['meta'], maxLen: 40);
  }

  return model;
}

List<Component> buildCardBriefingComponents({
  required List<int> sectionItemCounts,
}) {
  final rootChildren = <String>['heroCard', 'metricRow'];
  final components = <Component>[
    _component('root', 'Column', {
      'alignment': 'stretch',
      'children': rootChildren,
    }),
    _component('heroCard', 'Card', {'child': 'heroColumn'}),
    _component('heroColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['titleText', 'headlineText'],
    }),
    _textPath('titleText', '/title', usageHint: 'h3'),
    _textPath('headlineText', '/headline', usageHint: 'h5'),
    _component('metricRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['metric1Card', 'metric2Card', 'metric3Card'],
    }),
  ];

  for (var index = 1; index <= 3; index += 1) {
    components.addAll([
      _component('metric${index}Card', 'Card', {
        'child': 'metric${index}Column',
      }, weight: 1),
      _component('metric${index}Column', 'Column', {
        'alignment': 'stretch',
        'children': [
          'metric${index}LabelText',
          'metric${index}ValueText',
          'metric${index}DetailText',
        ],
      }),
      _textPath(
        'metric${index}LabelText',
        '/metric${index}Label',
        usageHint: 'caption',
      ),
      _textPath(
        'metric${index}ValueText',
        '/metric${index}Value',
        usageHint: 'h4',
      ),
      _textPath(
        'metric${index}DetailText',
        '/metric${index}Detail',
        usageHint: 'caption',
      ),
    ]);
  }

  for (
    var sectionIndex = 0;
    sectionIndex < sectionItemCounts.length;
    sectionIndex += 1
  ) {
    final sectionSlot = sectionIndex + 1;
    final itemCount = sectionItemCounts[sectionIndex];
    final sectionChildren = <String>[
      'section${sectionSlot}HeaderRow',
      'section${sectionSlot}HeaderDivider',
    ];
    final sectionCardId = 'section${sectionSlot}Card';

    components.addAll([
      _component(sectionCardId, 'Card', {
        'child': 'section${sectionSlot}Column',
      }),
      _component('section${sectionSlot}Column', 'Column', {
        'alignment': 'stretch',
        'children': sectionChildren,
      }),
      _component('section${sectionSlot}HeaderRow', 'Row', {
        'distribution': 'spaceBetween',
        'alignment': 'center',
        'children': [
          'section${sectionSlot}TitleText',
          'section${sectionSlot}CountText',
        ],
      }),
      _textPath(
        'section${sectionSlot}TitleText',
        '/section${sectionSlot}Title',
        usageHint: 'h4',
      ),
      _textPath(
        'section${sectionSlot}CountText',
        '/section${sectionSlot}CountText',
        usageHint: 'caption',
      ),
      _component('section${sectionSlot}HeaderDivider', 'Divider', const {}),
    ]);

    for (var itemIndex = 1; itemIndex <= itemCount; itemIndex += 1) {
      final itemId = 'section${sectionSlot}Item${itemIndex}Column';
      final topRowId = 'section${sectionSlot}Item${itemIndex}TopRow';
      final dividerId = 'section${sectionSlot}Item${itemIndex}Divider';
      components.addAll([
        _component(itemId, 'Column', {
          'alignment': 'stretch',
          'children': [
            topRowId,
            'section${sectionSlot}Item${itemIndex}DetailText',
          ],
        }),
        _component(topRowId, 'Row', {
          'distribution': 'spaceBetween',
          'alignment': 'center',
          'children': [
            'section${sectionSlot}Item${itemIndex}LabelText',
            'section${sectionSlot}Item${itemIndex}ValueText',
          ],
        }),
        _textPath(
          'section${sectionSlot}Item${itemIndex}LabelText',
          '/section${sectionSlot}Item${itemIndex}Label',
          usageHint: 'body',
        ),
        _textPath(
          'section${sectionSlot}Item${itemIndex}ValueText',
          '/section${sectionSlot}Item${itemIndex}Value',
          usageHint: 'caption',
        ),
        _textPath(
          'section${sectionSlot}Item${itemIndex}DetailText',
          '/section${sectionSlot}Item${itemIndex}Detail',
          usageHint: 'caption',
        ),
      ]);
      sectionChildren.add(itemId);
      if (itemIndex < itemCount) {
        components.add(_component(dividerId, 'Divider', const {}));
        sectionChildren.add(dividerId);
      }
    }

    rootChildren.add(sectionCardId);
  }

  rootChildren.addAll(['alertCard', 'footerCard']);
  components.addAll([
    _component('alertCard', 'Card', {'child': 'alertRow'}),
    _component('alertRow', 'Row', {
      'distribution': 'start',
      'alignment': 'start',
      'children': ['alertIcon', 'alertColumn'],
    }),
    _component('alertIcon', 'Icon', {
      'name': {'literalString': 'info'},
    }),
    _component('alertColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['alertTitleText', 'alertSummaryText', 'alertMetaText'],
    }, weight: 1),
    _textPath('alertTitleText', '/alertTitle', usageHint: 'caption'),
    _textPath('alertSummaryText', '/alertSummary'),
    _textPath('alertMetaText', '/alertMeta', usageHint: 'caption'),
    _component('footerCard', 'Card', {'child': 'footerText'}),
    _textPath('footerText', '/footer', usageHint: 'caption'),
  ]);

  return components;
}

Map<String, Object?> sportsBriefingMockContract() =>
    _decodeContract(_sportsMockJson);

Map<String, Object?> financeBriefingMockContract() =>
    _decodeContract(_financeMockJson);

Map<String, Object?> commuteBriefingMockContract() =>
    _decodeContract(_commuteMockJson);

Map<String, Object?> smartHomeBriefingMockContract() =>
    _decodeContract(_smartHomeMockJson);

Map<String, Object?> emergencyAlertBriefingMockContract() =>
    _decodeContract(_emergencyAlertMockJson);

Map<String, Object?> familyMessageBoardMockContract() =>
    _decodeContract(_familyMessageBoardMockJson);

Map<String, Object?> deliveryStatusMockContract() =>
    _decodeContract(_deliveryStatusMockJson);

Map<String, Object?> mediaCompanionMockContract() =>
    _decodeContract(_mediaCompanionMockJson);

Map<String, Object?> shoppingDecisionMockContract() =>
    _decodeContract(_shoppingDecisionMockJson);

Map<String, Object?> travelAssistantMockContract() =>
    _decodeContract(_travelAssistantMockJson);

Map<String, Object?> wellnessCardMockContract() =>
    _decodeContract(_wellnessCardMockJson);

Map<String, Object?> _decodeContract(String source) {
  return (jsonDecode(source) as Map).cast<String, Object?>();
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

String _cleanText(Object? value, {int maxLen = 80}) {
  final text = '${value ?? ''}'.trim().replaceAll(RegExp(r'\s+'), ' ');
  if (text.length <= maxLen) {
    return text;
  }
  return '${text.substring(0, maxLen - 3).trimRight()}...';
}

const String _sportsMockJson = '''
{
  "title": "오늘의 스포츠 브리핑",
  "headline": "현재 경기, 다음 경기, 순위 변동을 점수판처럼 크게 보여줍니다.",
  "primaryMetrics": [
    {"label": "메인 경기", "value": "서울 2 : 1 수원", "detail": "후반 68분"},
    {"label": "다음 경기", "value": "19:30 KBO", "detail": "잠실 구장"},
    {"label": "순위", "value": "2위", "detail": "1계단 상승"}
  ],
  "sections": [
    {
      "title": "메인 매치",
      "items": [
        {"label": "서울", "value": "2", "detail": "후반 52분 결승골"},
        {"label": "수원", "value": "1", "detail": "전반 21분 선제골"}
      ]
    },
    {
      "title": "오늘 경기와 순위",
      "items": [
        {"label": "다음 경기", "value": "19:30 LG vs 두산", "detail": "KBO 잠실 라이벌전"},
        {"label": "순위 변동", "value": "2위", "detail": "선두와 1경기 차"},
        {"label": "갱신", "value": "실시간", "detail": "최근 30초 이내 데이터"}
      ]
    }
  ],
  "alert": {
    "title": "지연 표기 필요",
    "summary": "실시간 경기로 보이는 순간 stale data에 매우 민감해지므로 갱신 주기를 항상 같이 보여주는 편이 좋습니다.",
    "meta": "라이브 스코어 정책"
  },
  "footer": "한국 사용자에게는 KBO, K리그, 주요 해외축구 순으로 우선 노출하는 편이 자연스럽습니다."
}
''';

const String _financeMockJson = '''
{
  "title": "시장 스냅샷",
  "headline": "KRW 기준 환율과 관심종목 움직임을 짧게 정리했습니다.",
  "primaryMetrics": [
    {"label": "코스피", "value": "+0.8%", "detail": "오전장 기준"},
    {"label": "USD/KRW", "value": "1,325.2", "detail": "전일 대비 -4.1"},
    {"label": "관심종목", "value": "3 / 5 상승", "detail": "반도체 강세"}
  ],
  "sections": [
    {
      "title": "관심종목",
      "items": [
        {"label": "삼성전자", "value": "+1.6%", "detail": "기관 매수세 유입"},
        {"label": "현대차", "value": "+0.9%", "detail": "환율 부담 완화 기대"},
        {"label": "NAVER", "value": "-0.4%", "detail": "광고 업황 관망"}
      ]
    },
    {
      "title": "참고 지표",
      "items": [
        {"label": "JPY/KRW", "value": "890.4", "detail": "원화 기준 강보합"},
        {"label": "예산 리마인드", "value": "이번 주 지출 68%", "detail": "주말 전 추가 지출 주의"}
      ]
    }
  ],
  "alert": {
    "title": "투자 조언 아님",
    "summary": "TV 금융 스냅샷은 참고용 정보로만 유지하고, 추천이나 매수 유도처럼 보이는 표현은 피하는 편이 안전합니다.",
    "meta": "금융 커뮤니케이션 정책"
  },
  "footer": "실제 운영 단계에서는 지연 시세 여부와 데이터 출처를 항상 표시하는 편이 좋습니다."
}
''';

const String _commuteMockJson = '''
{
  "title": "출근길 브리핑",
  "headline": "다음 일정까지 남은 시간과 추천 출발 시각을 먼저 보여줍니다.",
  "primaryMetrics": [
    {"label": "추천 출발", "value": "08:10", "detail": "지금부터 12분 후"},
    {"label": "예상 소요", "value": "42분", "detail": "평소보다 8분 증가"},
    {"label": "교통 리스크", "value": "보통", "detail": "강변북로 정체"}
  ],
  "sections": [
    {
      "title": "주요 경로",
      "items": [
        {"label": "대중교통", "value": "42분", "detail": "2호선 환승 포함, 지연 위험 보통"},
        {"label": "차량", "value": "55분", "detail": "강변북로 정체로 평균보다 느림"}
      ]
    },
    {
      "title": "대안",
      "items": [
        {"label": "대체 경로", "value": "43분", "detail": "버스 환승 1회, 도보 증가"},
        {"label": "도착 목표", "value": "08:55", "detail": "09:00 회의 전 5분 여유 확보"}
      ]
    }
  ],
  "alert": {
    "title": "실시간 변동 주의",
    "summary": "경로 추천은 짧은 시간에도 바뀔 수 있으니 마지막 갱신 시각을 반드시 함께 표시하세요.",
    "meta": "교통 데이터 정책"
  },
  "footer": "공용 TV에서는 집과 회사의 정확한 주소 대신 요약된 위치 표현을 사용하는 편이 안전합니다."
}
''';

const String _smartHomeMockJson = '''
{
  "title": "스마트홈 요약",
  "headline": "거실 TV에서 바로 확인해야 할 집 상태만 간단히 모았습니다.",
  "primaryMetrics": [
    {"label": "출입문", "value": "모두 잠김", "detail": "현관 포함 3개"},
    {"label": "실내 온도", "value": "22°", "detail": "쾌적"},
    {"label": "공기질", "value": "좋음", "detail": "미세먼지 낮음"}
  ],
  "sections": [
    {
      "title": "주요 공간",
      "items": [
        {"label": "현관문", "value": "잠김", "detail": "마지막 상태 변경 오전 6:55"},
        {"label": "거실 조명", "value": "꺼짐", "detail": "외출 모드 유지 중"},
        {"label": "공기청정기", "value": "자동", "detail": "현재 실내 공기질 좋음"}
      ]
    },
    {
      "title": "주의",
      "items": [
        {"label": "현관 카메라", "value": "움직임 1건", "detail": "새벽 5시 이후 추가 이벤트 없음"},
        {"label": "안방 습도", "value": "67%", "detail": "환기 권장 기준에 근접"}
      ]
    }
  ],
  "alert": {
    "title": "읽기 전용 권장",
    "summary": "스마트홈 요약 화면은 상태 확인 중심으로 유지하고, 실제 제어는 별도 승인 흐름으로 분리하는 편이 안전합니다.",
    "meta": "신뢰 경계"
  },
  "footer": "카메라와 가정 상태 정보는 민감할 수 있으므로 공용 화면 노출 범위를 보수적으로 유지합니다."
}
''';

const String _emergencyAlertMockJson = '''
{
  "title": "긴급 알림 모드",
  "headline": "영향 지역, 즉시 행동, 갱신 시각을 우선으로 보여줍니다.",
  "primaryMetrics": [
    {"label": "경보 수준", "value": "주의", "detail": "강풍 예비특보"},
    {"label": "영향 지역", "value": "서울 · 경기 서부", "detail": "오전 시간대"},
    {"label": "최종 갱신", "value": "07:10", "detail": "공식 소스 기준"}
  ],
  "sections": [
    {
      "title": "즉시 행동",
      "items": [
        {"icon": "warning", "label": "실외 이동", "value": "주의", "detail": "간판, 시설물 주변 통행 주의"},
        {"icon": "home", "label": "실내 안전", "value": "창문 점검", "detail": "강풍 대비 창문과 베란다 물건 고정"}
      ]
    },
    {
      "title": "추가 안내",
      "items": [
        {"icon": "place", "label": "영향 시간", "value": "오전 ~ 낮", "detail": "기상 변화에 따라 확대 가능"},
        {"icon": "sms", "label": "공식 확인", "value": "재난문자·기상청", "detail": "TV 화면은 참고용 표시 전용 모드"}
      ]
    }
  ],
  "alert": {
    "icon": "report",
    "title": "공식 소스 우선",
    "summary": "긴급 알림 화면은 공식 경보를 보조적으로 시각화하는 용도로만 사용하고, 불확실한 안내를 생성하지 않는 편이 안전합니다.",
    "meta": "재난 정보 정책"
  },
  "footer": "무경보 상태, 경보 상태, 소스 오류 상태 모두 시각적으로 구분되도록 설계하는 편이 좋습니다."
}
''';

const String _familyMessageBoardMockJson = '''
{
  "title": "가족 메시지 보드",
  "headline": "오늘 필요한 공지, 일정, 집안 메모를 한 화면에 정리했습니다.",
  "primaryMetrics": [
    {"label": "오늘 알림", "value": "4건", "detail": "학교 공지 1건"},
    {"label": "생일", "value": "1건", "detail": "저녁 케이크 예약"},
    {"label": "집안 할 일", "value": "3건", "detail": "분리수거 포함"}
  ],
  "sections": [
    {
      "title": "오늘 일정",
      "items": [
        {"icon": "school", "label": "학교 준비물", "value": "체육복", "detail": "등교 전 다시 확인 필요"},
        {"icon": "event", "label": "학원", "value": "17:30 수학", "detail": "도착 10분 전 출발 권장"}
      ]
    },
    {
      "title": "집안 메모",
      "items": [
        {"icon": "cake", "label": "오늘 생일", "value": "아빠", "detail": "저녁 7시 가족 식사 예정"},
        {"icon": "home", "label": "분리수거", "value": "오늘 밤", "detail": "현관 앞 박스 정리 필요"},
        {"icon": "notifications", "label": "학교 공지", "value": "가정통신문 확인", "detail": "내일까지 동의서 제출"}
      ]
    }
  ],
  "alert": {
    "icon": "familyRestroom",
    "title": "공유 범위 조절 필요",
    "summary": "가족 보드는 거실용 요약 화면으로 유지하고, 개인 메시지나 상세 일정은 별도 기기에서 보는 편이 안전합니다.",
    "meta": "가정용 프라이버시 기본값"
  },
  "footer": "학교 공지, 가족 일정, 집안 메모는 과하게 상세하지 않은 보드형 요약이 TV에 가장 잘 맞습니다."
}
''';

const String _deliveryStatusMockJson = '''
{
  "title": "배달 상태",
  "headline": "현재 단계와 예상 도착 시간을 중심으로 주문 상태를 크게 보여줍니다.",
  "primaryMetrics": [
    {"label": "예상 도착", "value": "18분", "detail": "평균 범위 15~20분"},
    {"label": "현재 단계", "value": "배달원 이동 중", "detail": "가게 출발 완료"},
    {"label": "주문 번호", "value": "#240315", "detail": "표시 범위 축약"}
  ],
  "sections": [
    {
      "title": "주문 진행",
      "items": [
        {"icon": "receiptLong", "label": "결제 완료", "value": "18:02", "detail": "주문 접수 완료"},
        {"icon": "restaurant", "label": "조리 완료", "value": "18:14", "detail": "픽업 대기 종료"},
        {"icon": "deliveryDining", "label": "이동 중", "value": "현재", "detail": "도착까지 약 18분 예상"}
      ]
    },
    {
      "title": "다음 액션",
      "items": [
        {"icon": "call", "label": "연락 필요 시", "value": "앱에서 진행", "detail": "TV에서는 번호 노출 없이 버튼만 제공 권장"},
        {"icon": "replay", "label": "재주문", "value": "가능", "detail": "자주 주문한 메뉴로 빠른 재선택 지원"}
      ]
    }
  ],
  "alert": {
    "icon": "privacyTip",
    "title": "PII 주의",
    "summary": "주소, 상세 연락처, 문 앞 요청사항은 거실 TV에 직접 노출하지 않는 편이 안전합니다.",
    "meta": "배송 정보 정책"
  },
  "footer": "실제 플랫폼 연동 전에는 mock 상태 머신으로 ETA와 단계 전환을 먼저 검증하는 것이 좋습니다."
}
''';

const String _mediaCompanionMockJson = '''
{
  "title": "미디어 컴패니언",
  "headline": "현재 보고 있는 장면과 관련된 핵심 정보만 옆 패널에 정리했습니다.",
  "primaryMetrics": [
    {"label": "현재 재생", "value": "32분", "detail": "에피소드 3"},
    {"label": "주요 출연진", "value": "3명", "detail": "등장 장면 기준"},
    {"label": "사운드트랙", "value": "1곡 확인", "detail": "현재 장면 배경음"}
  ],
  "sections": [
    {
      "title": "작품 정보",
      "items": [
        {"icon": "movie", "label": "작품", "value": "서울의 밤", "detail": "시즌 1 · 에피소드 3"},
        {"icon": "person", "label": "주요 인물", "value": "주연 3명", "detail": "현재 장면 등장 기준"}
      ]
    },
    {
      "title": "현재 장면",
      "items": [
        {"icon": "musicNote", "label": "OST", "value": "River Lights", "detail": "비슷한 분위기 재생 목록 연결 가능"},
        {"icon": "quiz", "label": "트리비아", "value": "촬영지 서울", "detail": "스포일러 없는 정보만 표시"}
      ]
    }
  ],
  "alert": {
    "icon": "visibility",
    "title": "스포일러 제어 필요",
    "summary": "미디어 컴패니언은 현재 장면 기준 정보만 보여주고 이후 전개를 암시하는 내용은 피하는 편이 안전합니다.",
    "meta": "콘텐츠 메타데이터 정책"
  },
  "footer": "재생 중인 콘텐츠 식별과 메타데이터 권리 처리가 해결되기 전까지는 mock companion 패널로 UX를 먼저 검증하는 것이 좋습니다."
}
''';

const String _shoppingDecisionMockJson = '''
{
  "title": "쇼핑 결정 지원",
  "headline": "거실 TV에서 바로 비교하고 결정할 수 있게 핵심 차이만 남겼습니다.",
  "primaryMetrics": [
    {"label": "비교 대상", "value": "3개", "detail": "65인치 TV"},
    {"label": "최저가", "value": "1,290,000원", "detail": "쿠폰 적용 기준"},
    {"label": "추천", "value": "1개", "detail": "가성비 우세"}
  ],
  "sections": [
    {
      "title": "핵심 비교",
      "items": [
        {"icon": "tv", "label": "모델 A", "value": "밝기 우수", "detail": "거실 시청 환경에 유리"},
        {"icon": "tv", "label": "모델 B", "value": "가격 우수", "detail": "가성비 중심 선택지"},
        {"icon": "tv", "label": "모델 C", "value": "사운드 우수", "detail": "별도 사운드바 없이도 무난"}
      ]
    },
    {
      "title": "판단 포인트",
      "items": [
        {"icon": "priceCheck", "label": "가격", "value": "모델 B 우세", "detail": "예산 민감하면 우선 고려"},
        {"icon": "star", "label": "리뷰", "value": "모델 A 안정적", "detail": "밝기와 색감 평가가 높음"}
      ]
    }
  ],
  "alert": {
    "icon": "balance",
    "title": "근거 기반 비교",
    "summary": "쇼핑 비교 화면은 광고성 문구보다 가격, 핵심 사양, 리뷰 요약 근거를 같이 보여주는 편이 신뢰에 유리합니다.",
    "meta": "상품 비교 정책"
  },
  "footer": "초기 PoC에서는 실제 오픈웹 검색보다 큐레이션된 비교 시나리오로 품질을 먼저 안정화하는 편이 좋습니다."
}
''';

const String _travelAssistantMockJson = '''
{
  "title": "여행 어시스턴트",
  "headline": "다음 이동과 예약 정보를 공항 출발 관점으로 정리했습니다.",
  "primaryMetrics": [
    {"label": "탑승까지", "value": "2시간 10분", "detail": "인천공항 T2"},
    {"label": "게이트", "value": "121", "detail": "변경 여부 확인 필요"},
    {"label": "공항 출발", "value": "40분 후", "detail": "교통 원활"}
  ],
  "sections": [
    {
      "title": "항공편",
      "items": [
        {"icon": "flightTakeoff", "label": "KE123", "value": "11:20 출발", "detail": "체크인 완료 · 탑승 시작 10:40"},
        {"icon": "luggage", "label": "수하물", "value": "1개 접수", "detail": "추가 수하물 없음"}
      ]
    },
    {
      "title": "체크포인트",
      "items": [
        {"icon": "security", "label": "보안검색", "value": "예상 12분", "detail": "현재 비교적 원활"},
        {"icon": "badge", "label": "예약 코드", "value": "A1B2**", "detail": "전체 코드는 별도 기기에서 확인 권장"}
      ]
    },
    {
      "title": "숙소",
      "items": [
        {"icon": "hotel", "label": "체크인", "value": "15:00", "detail": "도착 후 셀프 체크인 가능"}
      ]
    }
  ],
  "alert": {
    "icon": "update",
    "title": "운영 정보 변동 가능",
    "summary": "게이트, 탑승 시각, 공항 이동 시간은 짧은 시간에도 바뀔 수 있으니 마지막 갱신 시각을 함께 보여주는 편이 안전합니다.",
    "meta": "여정 운영 정보 정책"
  },
  "footer": "travel_app 레퍼런스를 재사용하기 좋은 시나리오지만, 예약 연동 전에는 mock itinerary로 화면 완성도를 먼저 검증하는 것이 좋습니다."
}
''';

const String _wellnessCardMockJson = '''
{
  "title": "오늘의 웰니스",
  "headline": "짧게 확인하고 바로 행동할 수 있는 건강 카드만 남겼습니다.",
  "primaryMetrics": [
    {"label": "수면", "value": "7시간 10분", "detail": "평균 대비 +20분"},
    {"label": "걸음 수", "value": "4,200보", "detail": "목표의 42%"},
    {"label": "추천", "value": "5분 스트레칭", "detail": "오전 루틴"}
  ],
  "sections": [
    {
      "title": "오늘 상태",
      "items": [
        {"icon": "bedtime", "label": "수면", "value": "양호", "detail": "기상 후 컨디션 안정"},
        {"icon": "directionsWalk", "label": "활동량", "value": "보통", "detail": "점심 전 10분 걷기 권장"}
      ]
    },
    {
      "title": "다음 행동",
      "items": [
        {"icon": "selfImprovement", "label": "스트레칭", "value": "5분", "detail": "목과 어깨 위주"},
        {"icon": "waterDrop", "label": "수분", "value": "한 컵 마시기", "detail": "오전 리마인드"}
      ]
    }
  ],
  "alert": {
    "icon": "healthAndSafety",
    "title": "비의료 정보 유지",
    "summary": "TV 웰니스 카드는 생활 습관 리마인드에 집중하고 의료적 조언처럼 보이는 표현은 피하는 편이 안전합니다.",
    "meta": "건강 데이터 정책"
  },
  "footer": "공용 거실 화면에서는 자세한 건강 수치보다 가벼운 행동 유도형 카드가 더 자연스럽습니다."
}
''';
