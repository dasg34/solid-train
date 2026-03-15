import 'dart:convert';

import 'package:genui/genui.dart';
import 'package:http/http.dart' as http;

import '../../home/models/template_surface.dart';

abstract interface class WeatherBriefingRepository {
  Future<Map<String, Object?>> fetchWeatherBriefing({
    String city = '서울',
    String district = '중구',
    int hours = 6,
  });
}

class WeatherTemplatePresenter implements TemplateSurfacePresenter {
  const WeatherTemplatePresenter({required this.repository, this.hours = 6});

  final WeatherBriefingRepository repository;
  final int hours;

  @override
  TemplateSurfaceStyle get surfaceStyle =>
      TemplateSurfaceStyle.atmosphericWeather;

  @override
  TemplateSurfacePayload? buildLoading() {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '창밖 공기를 정리하는 중',
        detail: '서울 실시간 날씨를 불러오고 있습니다.',
        hint: '현재 기온, 하늘 상태, 시간대별 흐름을 순서대로 채웁니다.',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  TemplateSurfacePayload buildError(Object error) {
    return TemplateSurfacePayload(
      components: buildTemplateStatusComponents(),
      dataModel: buildTemplateStatusModel(
        title: '날씨를 불러오지 못했습니다',
        detail: '실시간 데이터를 가져오지 못했어요.',
        hint: '$error',
      ),
      surfaceStyle: surfaceStyle,
    );
  }

  @override
  Future<TemplateSurfacePayload> load() async {
    final dataModel = await repository.fetchWeatherBriefing(hours: hours);
    return TemplateSurfacePayload(
      components: buildWeatherBriefingComponents(hours: hours),
      dataModel: dataModel,
      surfaceStyle: surfaceStyle,
    );
  }
}

class OpenMeteoWeatherRepository implements WeatherBriefingRepository {
  const OpenMeteoWeatherRepository();

  @override
  Future<Map<String, Object?>> fetchWeatherBriefing({
    String city = '서울',
    String district = '중구',
    int hours = 6,
  }) async {
    final uri = Uri.https('api.open-meteo.com', '/v1/forecast', {
      'latitude': '37.5665',
      'longitude': '126.9780',
      'timezone': 'Asia/Seoul',
      'current': [
        'temperature_2m',
        'apparent_temperature',
        'relative_humidity_2m',
        'precipitation',
        'weather_code',
      ].join(','),
      'hourly': [
        'temperature_2m',
        'precipitation_probability',
        'weather_code',
      ].join(','),
      'forecast_hours': hours.clamp(1, 24).toString(),
    });

    final response = await http.get(uri).timeout(const Duration(seconds: 6));
    if (response.statusCode != 200) {
      throw Exception('날씨 API 응답 실패 (${response.statusCode})');
    }

    final json = jsonDecode(response.body);
    if (json is! Map<String, dynamic>) {
      throw Exception('날씨 API 응답 형식이 올바르지 않습니다.');
    }

    final current = (json['current'] as Map?)?.cast<String, Object?>() ?? {};
    final hourly = (json['hourly'] as Map?)?.cast<String, Object?>() ?? {};
    final hourlyTimes =
        (hourly['time'] as List?)?.cast<Object?>() ?? const <Object?>[];
    final hourlyTemps =
        (hourly['temperature_2m'] as List?)?.cast<Object?>() ??
        const <Object?>[];
    final hourlyPrecip =
        (hourly['precipitation_probability'] as List?)?.cast<Object?>() ??
        const <Object?>[];
    final hourlyCodes =
        (hourly['weather_code'] as List?)?.cast<Object?>() ?? const <Object?>[];

    final normalizedHours = <Map<String, Object?>>[];
    for (
      var index = 0;
      index < hourlyTimes.length && index < hours;
      index += 1
    ) {
      normalizedHours.add({
        'time': hourlyTimes[index],
        'temperature_c': index < hourlyTemps.length ? hourlyTemps[index] : null,
        'precip_probability_pct': index < hourlyPrecip.length
            ? hourlyPrecip[index]
            : null,
        'condition': _openMeteoCondition(
          index < hourlyCodes.length ? hourlyCodes[index] : null,
        ),
      });
    }

    final currentCondition = _openMeteoCondition(current['weather_code']);
    final firstPrecip = normalizedHours.isEmpty
        ? null
        : normalizedHours.first['precip_probability_pct'];
    final currentTemperature = _parseDouble(current['temperature_2m']);
    final apparentTemperature = _parseDouble(current['apparent_temperature']);
    final humidity = _parseDouble(current['relative_humidity_2m']);
    final precipitation = _parseDouble(firstPrecip);
    final title = district.isEmpty ? city : '$city $district';

    final model = <String, Object?>{
      'title': title,
      'regionKicker': district.isEmpty ? '$city 실황 브리핑' : '$city $district 실황',
      'moodLine': _cleanText(
        _buildMoodLine(
          condition: currentCondition,
          temperature: currentTemperature,
          apparentTemperature: apparentTemperature,
        ),
        maxLen: 56,
      ),
      'currentTemp': _formatTemperature(currentTemperature),
      'condition': currentCondition,
      'feelsLikeValue': _formatTemperature(apparentTemperature),
      'precipitationValue': _formatPercent(precipitation),
      'humidityValue': _formatPercent(humidity),
      'updatedAt': _formatUpdatedAt(current['time']?.toString()),
      'sourceLine': '$title · Open-Meteo 실황/예보',
      'hourlyTitle': normalizedHours.isEmpty
          ? '시간대별 흐름'
          : '앞으로 ${normalizedHours.length}시간 흐름',
      'footerTitle': '데이터 메모',
    };

    for (var index = 0; index < normalizedHours.length; index += 1) {
      final hour = normalizedHours[index];
      final item = index + 1;
      model['hour${item}Time'] = _formatHourLabel(hour['time']?.toString());
      model['hour${item}Condition'] = _cleanText(hour['condition'], maxLen: 12);
      model['hour${item}Temp'] = _formatTemperature(hour['temperature_c']);
      model['hour${item}Rain'] = _formatPercent(hour['precip_probability_pct']);
    }

    return model;
  }
}

List<Component> buildWeatherBriefingComponents({int hours = 6}) {
  final hourlyChildren = <String>[
    'hourlyTitle',
    'hourlyHeaderRow',
    'hourlyHeaderDivider',
  ];

  final components = <Component>[
    _component('root', 'Column', {
      'alignment': 'stretch',
      'children': ['heroCard', 'metricRow', 'hourlyCard', 'footerCard'],
    }),
    _component('heroCard', 'Card', {'child': 'heroColumn'}),
    _component('heroColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['heroTopRow', 'moodLineText', 'heroDivider', 'currentRow'],
    }),
    _component('heroTopRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['locationColumn', 'updatedColumn'],
    }),
    _component('locationColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['regionKickerText', 'locationText'],
    }, weight: 4),
    _textPath('regionKickerText', '/regionKicker', usageHint: 'caption'),
    _textPath('locationText', '/title', usageHint: 'h3'),
    _component('updatedColumn', 'Column', {
      'alignment': 'end',
      'children': ['updatedLabel', 'updatedText'],
    }, weight: 3),
    _textLiteral('updatedLabel', '업데이트', usageHint: 'caption'),
    _textPath('updatedText', '/updatedAt', usageHint: 'caption'),
    _textPath('moodLineText', '/moodLine', usageHint: 'h2'),
    _component('heroDivider', 'Divider', const {}),
    _component('currentRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['temperatureColumn', 'conditionColumn'],
    }),
    _component('temperatureColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['temperatureKicker', 'temperatureText'],
    }, weight: 3),
    _textLiteral('temperatureKicker', '지금 온도', usageHint: 'caption'),
    _textPath('temperatureText', '/currentTemp', usageHint: 'h1'),
    _component('conditionColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['conditionKicker', 'conditionText'],
    }, weight: 4),
    _textLiteral('conditionKicker', '하늘 상태', usageHint: 'caption'),
    _textPath('conditionText', '/condition', usageHint: 'h3'),
    _component('metricRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'start',
      'children': ['feelsCard', 'humidityCard', 'rainCard'],
    }),
    _component('feelsCard', 'Card', {'child': 'feelsCardColumn'}, weight: 1),
    _component('feelsCardColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['feelsCardLabel', 'feelsCardValue'],
    }),
    _textLiteral('feelsCardLabel', '체감 온도', usageHint: 'caption'),
    _textPath('feelsCardValue', '/feelsLikeValue', usageHint: 'h4'),
    _component('humidityCard', 'Card', {
      'child': 'humidityCardColumn',
    }, weight: 1),
    _component('humidityCardColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['humidityCardLabel', 'humidityCardValue'],
    }),
    _textLiteral('humidityCardLabel', '습도', usageHint: 'caption'),
    _textPath('humidityCardValue', '/humidityValue', usageHint: 'h4'),
    _component('rainCard', 'Card', {'child': 'rainCardColumn'}, weight: 1),
    _component('rainCardColumn', 'Column', {
      'alignment': 'stretch',
      'children': ['rainCardLabel', 'rainCardValue'],
    }),
    _textLiteral('rainCardLabel', '강수확률', usageHint: 'caption'),
    _textPath('rainCardValue', '/precipitationValue', usageHint: 'h4'),
    _component('hourlyCard', 'Card', {'child': 'hourlyColumn'}),
    _component('hourlyColumn', 'Column', {
      'alignment': 'stretch',
      'children': hourlyChildren,
    }),
    _textPath('hourlyTitle', '/hourlyTitle', usageHint: 'h4'),
    _component('hourlyHeaderRow', 'Row', {
      'distribution': 'spaceBetween',
      'alignment': 'center',
      'children': [
        'hourlyHeaderTime',
        'hourlyHeaderCondition',
        'hourlyHeaderRain',
        'hourlyHeaderTemp',
      ],
    }),
    _textLiteral('hourlyHeaderTime', '시간', usageHint: 'caption', weight: 2),
    _textLiteral(
      'hourlyHeaderCondition',
      '하늘',
      usageHint: 'caption',
      weight: 3,
    ),
    _textLiteral('hourlyHeaderRain', '비', usageHint: 'caption', weight: 2),
    _textLiteral('hourlyHeaderTemp', '기온', usageHint: 'caption', weight: 1),
    _component('hourlyHeaderDivider', 'Divider', const {}),
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
      'children': ['footerTitleText', 'sourceLineText'],
    }, weight: 1),
    _textPath('footerTitleText', '/footerTitle', usageHint: 'caption'),
    _textPath('sourceLineText', '/sourceLine', usageHint: 'caption'),
  ];

  for (var index = 1; index <= hours.clamp(1, 6); index += 1) {
    final rowId = 'hour${index}Row';
    final dividerId = 'hour${index}Divider';
    final timeId = 'hour${index}TimeText';
    final conditionId = 'hour${index}ConditionText';
    final rainId = 'hour${index}RainText';
    final tempId = 'hour${index}TempText';
    components.addAll([
      _component(rowId, 'Row', {
        'distribution': 'spaceBetween',
        'alignment': 'center',
        'children': [timeId, conditionId, rainId, tempId],
      }),
      _textPath(timeId, '/hour${index}Time', usageHint: 'h5', weight: 2),
      _textPath(
        conditionId,
        '/hour${index}Condition',
        usageHint: 'body',
        weight: 3,
      ),
      _textPath(rainId, '/hour${index}Rain', usageHint: 'h5', weight: 2),
      _textPath(tempId, '/hour${index}Temp', usageHint: 'h5', weight: 1),
    ]);
    hourlyChildren.add(rowId);
    if (index < hours.clamp(1, 6)) {
      components.add(_component(dividerId, 'Divider', const {}));
      hourlyChildren.add(dividerId);
    }
  }

  return components;
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

String _formatTemperature(Object? value) {
  final parsed = _parseDouble(value);
  if (parsed == null) {
    return '-';
  }
  return '${parsed.round()}°';
}

String _formatPercent(Object? value) {
  final parsed = _parseDouble(value);
  if (parsed == null) {
    return '-';
  }
  return '${parsed.round()}%';
}

double? _parseDouble(Object? value) {
  return double.tryParse('$value');
}

String _formatUpdatedAt(String? value) {
  if (value == null || value.isEmpty) {
    return '업데이트 시각 확인 필요';
  }
  final dateTime = DateTime.tryParse(value);
  if (dateTime == null) {
    return '업데이트 시각 확인 필요';
  }
  final meridiem = dateTime.hour < 12 ? '오전' : '오후';
  final hour = dateTime.hour % 12 == 0 ? 12 : dateTime.hour % 12;
  return '$meridiem $hour:${dateTime.minute.toString().padLeft(2, '0')} 기준';
}

String _formatHourLabel(String? value) {
  if (value == null || value.isEmpty) {
    return '-';
  }
  final dateTime = DateTime.tryParse(value);
  if (dateTime == null) {
    return '-';
  }
  return '${dateTime.hour.toString().padLeft(2, '0')}시';
}

String _openMeteoCondition(Object? code) {
  const mapping = {
    0: '맑음',
    1: '대체로 맑음',
    2: '구름 조금',
    3: '흐림',
    45: '안개',
    48: '짙은 안개',
    51: '약한 이슬비',
    53: '이슬비',
    55: '강한 이슬비',
    61: '약한 비',
    63: '비',
    65: '강한 비',
    71: '약한 눈',
    73: '눈',
    75: '강한 눈',
    80: '약한 소나기',
    81: '소나기',
    82: '강한 소나기',
    95: '뇌우',
    96: '우박 가능 뇌우',
    99: '강한 우박 가능 뇌우',
  };
  final parsed = int.tryParse('$code');
  return mapping[parsed] ?? '날씨 정보';
}

String _buildMoodLine({
  required String condition,
  required double? temperature,
  required double? apparentTemperature,
}) {
  final feelsCold = (apparentTemperature ?? temperature ?? 0) <= 4;
  if (condition.contains('맑')) {
    return feelsCold
        ? '빛은 맑지만 공기는 아직 차갑게 남아 있는 서울의 아침입니다.'
        : '햇빛이 은은하게 번지며 하루를 부드럽게 여는 아침입니다.';
  }
  if (condition.contains('흐림') || condition.contains('구름')) {
    return feelsCold
        ? '회색 구름이 낮게 머문, 차분하고 선선한 서울의 아침입니다.'
        : '빛이 눌린 하늘 덕분에 도심이 한층 잔잔하게 느껴집니다.';
  }
  if (condition.contains('비') || condition.contains('소나기')) {
    return '빗기운이 감도는 하늘이라 걸음과 시선이 조금 느려지는 아침입니다.';
  }
  if (condition.contains('눈')) {
    return '차갑고 고요한 공기가 도심 위에 얇게 내려앉은 아침입니다.';
  }
  return '오늘 서울의 공기를 큰 화면에 차분하게 정리했습니다.';
}
