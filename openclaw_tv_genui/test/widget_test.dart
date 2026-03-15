import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/app/openclaw_tv_app.dart';
import 'package:openclaw_tv_genui/features/card_briefing/data/mock_card_briefing_presenter.dart';
import 'package:openclaw_tv_genui/features/home/models/template_registry.dart';
import 'package:openclaw_tv_genui/features/news/data/news_briefing_repository.dart';
import 'package:openclaw_tv_genui/features/schedule/data/schedule_briefing_repository.dart';
import 'package:openclaw_tv_genui/features/weather/data/weather_briefing_repository.dart';

class FakeWeatherBriefingRepository implements WeatherBriefingRepository {
  @override
  Future<Map<String, Object?>> fetchWeatherBriefing({
    String city = '서울',
    String district = '중구',
    int hours = 6,
  }) async {
    return {
      'title': '$city $district',
      'regionKicker': '$city $district 실황',
      'moodLine': '회색 구름이 낮게 머문, 차분하고 선선한 서울의 아침입니다.',
      'currentTemp': '7°',
      'condition': '흐림',
      'feelsLikeValue': '4°',
      'precipitationValue': '30%',
      'humidityValue': '61%',
      'updatedAt': '오전 8:10 기준',
      'sourceLine': '$city $district · Fake Weather Repo',
      'hourlyTitle': '앞으로 6시간 흐름',
      'footerTitle': '데이터 메모',
      'footer': '테스트용 날씨 저장소를 사용 중입니다.',
      'hour1Time': '09시',
      'hour1Condition': '흐림',
      'hour1Temp': '7°',
      'hour1Rain': '30%',
      'hour2Time': '10시',
      'hour2Condition': '흐림',
      'hour2Temp': '8°',
      'hour2Rain': '20%',
      'hour3Time': '11시',
      'hour3Condition': '흐림',
      'hour3Temp': '9°',
      'hour3Rain': '10%',
      'hour4Time': '12시',
      'hour4Condition': '구름 조금',
      'hour4Temp': '10°',
      'hour4Rain': '10%',
      'hour5Time': '13시',
      'hour5Condition': '대체로 맑음',
      'hour5Temp': '11°',
      'hour5Rain': '0%',
      'hour6Time': '14시',
      'hour6Condition': '대체로 맑음',
      'hour6Temp': '12°',
      'hour6Rain': '0%',
    };
  }
}

class FakeNewsBriefingRepository implements NewsBriefingRepository {
  @override
  Future<Map<String, Object?>> fetchNewsBriefing({int count = 6}) async {
    return {
      'title': '오늘의 주요 뉴스',
      'headline': '테스트 리드 기사입니다.',
      'updatedAt': '오전 8:20 기준',
      'publisherValue': '연합뉴스TV',
      'publisherDetail': '오전 8:20 기준',
      'headlineMetricValue': '6건',
      'headlineMetricDetail': '최신 RSS 피드',
      'breakingMetricValue': '1건',
      'breakingMetricDetail': '제목 기준 속보',
      'leadLabel': '정치',
      'leadValue': '테스트 리드 기사입니다.',
      'leadDetail': '오전 8:15 · 연합뉴스TV',
      'headlineSectionTitle': '최신 헤드라인',
      'secondaryCount': 2,
      'footerTitle': '데이터 메모',
      'footer': '연합뉴스TV RSS 기준 · 헤드라인과 갱신 시각을 함께 표시합니다.',
      'headline1Label': '경제',
      'headline1Value': '첫 번째 테스트 헤드라인',
      'headline1Detail': '오전 8:10 · 연합뉴스TV',
      'headline2Label': '사회',
      'headline2Value': '두 번째 테스트 헤드라인',
      'headline2Detail': '오전 8:05 · 연합뉴스TV',
    };
  }
}

class FakeScheduleBriefingRepository implements ScheduleBriefingRepository {
  @override
  Future<Map<String, Object?>> fetchScheduleBriefing({
    int windowDays = 120,
    int maxEvents = 6,
    DateTime? now,
  }) async {
    return {
      'title': '오늘 일정 브리핑',
      'headline': '다음 일정은 오전 10:30 제품 데모 리허설입니다.',
      'updatedAt': '오전 8:30 기준',
      'nextMetricValue': '오전 10:30 제품 데모 리허설',
      'nextMetricDetail': '3월 15일 · 오전 10:30-오전 11:30 · 회의실 A',
      'todayMetricValue': '3건',
      'todayMetricDetail': '3월 15일',
      'currentMetricValue': '없음',
      'currentMetricDetail': '현재 진행 중인 일정 없음',
      'focusTitle': '지금과 다음',
      'currentLabel': '현재',
      'currentValue': '진행 중 일정 없음',
      'currentDetail': '다음 일정만 준비하면 됩니다.',
      'nextLabel': '다음',
      'nextValue': '제품 데모 리허설',
      'nextDetail': '3월 15일 · 오전 10:30-오전 11:30 · 회의실 A',
      'eventsTitle': '오늘 일정',
      'eventsCountText': '3건',
      'noticeTitle': '공용 화면 주의',
      'noticeSummary': '참석자 이름과 상세 메모는 요약형으로만 보여줍니다.',
      'noticeMeta': 'ICS 일정 요약',
      'footerTitle': '데이터 메모',
      'footerText': '테스트용 일정 저장소를 사용 중입니다.',
      'event1Label': '제품 데모 리허설',
      'event1Value': '3월 15일 오전 10:30',
      'event1Detail': '3월 15일 · 오전 10:30-오전 11:30 · 회의실 A',
      'event2Label': '파트너 점심 미팅',
      'event2Value': '3월 15일 오후 1:00',
      'event2Detail': '3월 15일 · 오후 1:00-오후 2:00 · 을지로',
    };
  }
}

class FakeCardBriefingRepository implements CardBriefingRepository {
  const FakeCardBriefingRepository(this.contract);

  final Map<String, Object?> contract;

  @override
  Future<Map<String, Object?>> fetchBriefing() async => contract;
}

void main() {
  testWidgets('renders Korean TV mock scenarios and switches previews', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(1920, 1080);
    tester.view.devicePixelRatio = 1.0;
    addTearDown(() {
      tester.view.resetPhysicalSize();
      tester.view.resetDevicePixelRatio();
    });

    await tester.pumpWidget(
      OpenclawTvApp(
        templateRegistry: buildDefaultTemplateRegistry(
          weatherRepository: FakeWeatherBriefingRepository(),
          newsRepository: FakeNewsBriefingRepository(),
          scheduleRepository: FakeScheduleBriefingRepository(),
          dailyRepository: const FakeCardBriefingRepository({
            'title': '오늘의 브리핑',
            'headline': '날씨, 일정, 뉴스, 출근 정보를 아침용 카드로 묶었습니다.',
            'primaryMetrics': [
              {'label': '현재 기온', 'value': '7°', 'detail': '출근길 선선함'},
              {'label': '첫 일정', 'value': '오전 10:30 제품 데모', 'detail': '회의실 A'},
              {'label': '출발 권장', 'value': '08:10', 'detail': '예상 42분'},
            ],
            'sections': [
              {
                'title': '오늘 날씨',
                'items': [
                  {'label': '오전', 'value': '흐림 7°', 'detail': '강수확률 30%'},
                ],
              },
              {
                'title': '다음 일정',
                'items': [
                  {'label': '10:30', 'value': '제품 데모 리허설', 'detail': '회의실 A'},
                ],
              },
              {
                'title': '주요 뉴스',
                'items': [
                  {
                    'label': '경제',
                    'value': '첫 번째 테스트 헤드라인',
                    'detail': '시장 관망세 유지',
                  },
                ],
              },
              {
                'title': '출근',
                'items': [
                  {
                    'label': '대중교통',
                    'value': '42분',
                    'detail': '지금부터 12분 후 출발 권장',
                  },
                ],
              },
            ],
            'alert': {
              'title': '부분 실패 허용',
              'summary': '일부 카드가 비어도 전체 브리핑은 유지합니다.',
              'meta': '오케스트레이션 정책',
            },
            'footer': '테스트용 데일리 브리핑 저장소를 사용 중입니다.',
          }),
        ),
      ),
    );
    await tester.pumpAndSettle();

    expect(find.text('OpenClaw TV'), findsOneWidget);
    expect(find.text('날씨 브리핑'), findsOneWidget);
    expect(find.text('서울 중구'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '뉴스 보여줘');
    await tester.tap(find.text('주제에 맞는 템플릿 선택'));
    await tester.pumpAndSettle();

    expect(find.text('오늘의 주요 뉴스'), findsOneWidget);
    expect(find.text('첫 번째 테스트 헤드라인'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '일정 보여줘');
    await tester.tap(find.text('주제에 맞는 템플릿 선택'));
    await tester.pumpAndSettle();

    expect(find.text('오늘 일정 브리핑'), findsOneWidget);
    expect(find.textContaining('파트너 점심 미팅'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '오늘 브리핑 보여줘');
    await tester.tap(find.text('주제에 맞는 템플릿 선택'));
    await tester.pumpAndSettle();

    expect(find.text('오늘의 브리핑'), findsOneWidget);
    expect(find.text('출근'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '출근길 보여줘');
    await tester.tap(find.text('주제에 맞는 템플릿 선택'));
    await tester.pumpAndSettle();

    expect(find.text('추천 출발'), findsOneWidget);
    expect(find.text('교통 리스크'), findsOneWidget);

    await tester.enterText(find.byType(TextField), '여행 일정 보여줘');
    await tester.tap(find.text('주제에 맞는 템플릿 선택'));
    await tester.pumpAndSettle();

    expect(find.text('탑승까지'), findsOneWidget);
    expect(find.text('항공편'), findsOneWidget);
  });
}
