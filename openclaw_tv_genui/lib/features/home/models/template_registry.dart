import 'dart:math' as math;

import '../../card_briefing/data/mock_card_briefing_presenter.dart';
import '../../news/data/news_briefing_repository.dart';
import '../../schedule/data/schedule_briefing_repository.dart';
import '../../weather/data/weather_briefing_repository.dart';
import 'template_scenario.dart';
import 'template_surface.dart';

const String tvPocCatalogId = 'openclaw_tv_poc_catalog';
const String previewSurfaceId = 'tv_preview_surface';

class TemplateDefinition {
  const TemplateDefinition({required this.scenario, required this.presenter});

  final TemplateScenario scenario;
  final TemplateSurfacePresenter presenter;
}

class TemplateRegistry {
  TemplateRegistry({required List<TemplateDefinition> templates})
    : _templates = List<TemplateDefinition>.unmodifiable(templates),
      scenarios = List<TemplateScenario>.unmodifiable(
        templates.map((template) => template.scenario),
      );

  final List<TemplateDefinition> _templates;
  final List<TemplateScenario> scenarios;

  TemplateScenario route(String prompt) {
    final normalizedPrompt = prompt.trim().toLowerCase();
    TemplateDefinition? bestMatch;
    var bestKeywordLength = -1;
    var bestMatchCount = -1;

    for (final template in _templates) {
      final matches = template.scenario.keywords
          .where(normalizedPrompt.contains)
          .toList();
      if (matches.isEmpty) {
        continue;
      }

      final longestKeyword = matches.fold<int>(
        0,
        (longest, keyword) => math.max(longest, keyword.length),
      );
      if (longestKeyword > bestKeywordLength ||
          (longestKeyword == bestKeywordLength &&
              matches.length > bestMatchCount)) {
        bestMatch = template;
        bestKeywordLength = longestKeyword;
        bestMatchCount = matches.length;
      }
    }

    return bestMatch?.scenario ?? scenarios.first;
  }

  TemplateSurfacePresenter presenterFor(String scenarioId) {
    return definitionFor(scenarioId).presenter;
  }

  TemplateDefinition definitionFor(String scenarioId) {
    for (final template in _templates) {
      if (template.scenario.id == scenarioId) {
        return template;
      }
    }
    return _templates.first;
  }
}

TemplateRegistry buildDefaultTemplateRegistry({
  WeatherBriefingRepository weatherRepository =
      const OpenMeteoWeatherRepository(),
  NewsBriefingRepository newsRepository = const YonhapNewsBriefingRepository(),
  ScheduleBriefingRepository scheduleRepository =
      const IcsScheduleBriefingRepository(),
  CardBriefingRepository? dailyRepository,
  CardBriefingRepository? sportsRepository,
  CardBriefingRepository? financeRepository,
}) {
  final resolvedDailyRepository =
      dailyRepository ??
      const HttpCardBriefingRepository(
        endpointUrl: 'http://127.0.0.1:3001/daily-briefing',
      );
  final resolvedSportsRepository =
      sportsRepository ??
      const HttpCardBriefingRepository(
        endpointUrl: 'http://127.0.0.1:3001/sports-briefing',
      );
  final resolvedFinanceRepository =
      financeRepository ??
      const HttpCardBriefingRepository(
        endpointUrl: 'http://127.0.0.1:3001/finance-briefing',
      );

  return TemplateRegistry(
    templates: [
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'weather',
          title: '날씨 브리핑',
          prompt: '날씨 보여줘',
          summary: '큰 화면에서 한눈에 들어오는 몰입형 날씨 surface 예시',
          pattern: TvSurfacePattern.immersive,
          keywords: ['오늘 날씨', '날씨 보여줘', '비 와', '날씨', '기온', '습도'],
        ),
        presenter: WeatherTemplatePresenter(repository: weatherRepository),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'news',
          title: '뉴스 브리핑',
          prompt: '뉴스 보여줘',
          summary: '한국 주요 헤드라인을 패널형으로 보여주는 뉴스 surface 예시',
          pattern: TvSurfacePattern.sidePanel,
          keywords: ['뉴스 보여줘', '오늘 뉴스 요약', '헤드라인 보여줘', '뉴스', '속보'],
        ),
        presenter: NewsTemplatePresenter(repository: newsRepository),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'schedule',
          title: '일정 요약',
          prompt: '일정 보여줘',
          summary: '한국 기준 일정과 다음 약속을 요약하는 일정 surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: ['일정 보여줘', '오늘 일정', '다음 약속', '캘린더', '미팅', '회의'],
        ),
        presenter: ScheduleTemplatePresenter(repository: scheduleRepository),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'daily',
          title: '데일리 브리핑',
          prompt: '오늘 브리핑 보여줘',
          summary: '날씨, 뉴스, 일정, 출근 요약을 한 화면에 묶는 아침 브리핑 예시',
          pattern: TvSurfacePattern.immersive,
          keywords: ['오늘 브리핑 보여줘', '오늘 브리핑', '아침 요약해줘', '데일리 브리핑'],
        ),
        presenter: LiveCardBriefingPresenter(
          title: '데일리 브리핑',
          repository: resolvedDailyRepository,
          loadingTitle: '오늘 브리핑 준비 중',
          loadingDetail: '날씨, 뉴스, 일정, 출근 카드를 순서대로 조합하고 있습니다.',
          loadingHint: '일부 데이터가 늦어도 남은 카드부터 먼저 보여줍니다.',
          errorTitle: '오늘 브리핑을 불러오지 못했습니다',
          errorDetail: '로컬 브리핑 프록시 또는 compose-live skill 상태를 확인해 주세요.',
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'sports',
          title: '스포츠 브리핑',
          prompt: '스포츠 보여줘',
          summary: '점수판과 경기 요약을 카드형으로 보여주는 스포츠 surface 예시',
          pattern: TvSurfacePattern.sidePanel,
          keywords: ['스포츠 보여줘', '오늘 경기 알려줘', '축구 스코어 보여줘', '스포츠', '야구', '축구'],
        ),
        presenter: LiveCardBriefingPresenter(
          title: '스포츠 브리핑',
          repository: resolvedSportsRepository,
          loadingTitle: '스포츠 브리핑 준비 중',
          loadingDetail: '한국 기준 경기 결과와 다음 일정을 정리하고 있습니다.',
          loadingHint: '메인 경기, 다음 경기, 순위 흐름부터 먼저 채웁니다.',
          errorTitle: '스포츠 브리핑을 불러오지 못했습니다',
          errorDetail: '로컬 브리핑 프록시 또는 sports live skill 상태를 확인해 주세요.',
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'finance',
          title: '금융 스냅샷',
          prompt: '시장 보여줘',
          summary: '환율과 관심 종목을 카드형으로 요약하는 금융 surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: ['시장 요약해줘', '환율 알려줘', '주식 보여줘', '시장 보여줘', '금융', '환율'],
        ),
        presenter: LiveCardBriefingPresenter(
          title: '금융 스냅샷',
          repository: resolvedFinanceRepository,
          loadingTitle: '금융 스냅샷 준비 중',
          loadingDetail: '한국 시장 지표와 환율 스냅샷을 정리하고 있습니다.',
          loadingHint: '지수, 환율, 관심 종목 흐름을 카드형으로 모읍니다.',
          errorTitle: '금융 스냅샷을 불러오지 못했습니다',
          errorDetail: '로컬 브리핑 프록시 또는 finance live skill 상태를 확인해 주세요.',
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'commute',
          title: '출근길 브리핑',
          prompt: '출근길 보여줘',
          summary: '추천 출발 시각과 경로를 카드형으로 보여주는 commute surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: ['출근길 보여줘', '언제 출발해야 해', '교통 상황 알려줘', '출근', '퇴근', '경로'],
        ),
        presenter: MockCardBriefingPresenter(
          title: '출근길 브리핑',
          contract: commuteBriefingMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'smart_home',
          title: '스마트홈 요약',
          prompt: '집 상태 보여줘',
          summary: '문, 조명, 실내 상태를 카드형으로 모아보는 home surface 예시',
          pattern: TvSurfacePattern.sidePanel,
          keywords: ['집 상태 보여줘', '스마트홈 요약해줘', '문 잠겼는지 보여줘', '스마트홈', '조명'],
        ),
        presenter: MockCardBriefingPresenter(
          title: '스마트홈 요약',
          contract: smartHomeBriefingMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'emergency',
          title: '긴급 알림 모드',
          prompt: '긴급 알림 보여줘',
          summary: '영향 지역과 즉시 행동을 카드형으로 보여주는 알림 surface 예시',
          pattern: TvSurfacePattern.immersive,
          keywords: ['긴급 알림 보여줘', '재난 정보 보여줘', '경보 모드 켜줘', '긴급', '재난', '경보'],
        ),
        presenter: MockCardBriefingPresenter(
          title: '긴급 알림 모드',
          contract: emergencyAlertBriefingMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'family',
          title: '가족 메시지 보드',
          prompt: '가족 보드 보여줘',
          summary: '가족 공지와 집안 메모를 카드형으로 정리하는 보드 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: [
            '가족 보드 보여줘',
            '집안 메모 보여줘',
            '오늘 가족 일정 뭐야',
            '가족 일정',
            '가족',
            '집안 메모',
          ],
        ),
        presenter: MockCardBriefingPresenter(
          title: '가족 메시지 보드',
          contract: familyMessageBoardMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'delivery',
          title: '배달 상태',
          prompt: '배달 어디쯤이야?',
          summary: 'ETA와 주문 단계를 카드형으로 보여주는 배달 surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: ['배달 어디쯤이야', '주문 상태 보여줘', '음식 언제 와', '배달', '주문 상태'],
        ),
        presenter: MockCardBriefingPresenter(
          title: '배달 상태',
          contract: deliveryStatusMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'media',
          title: '미디어 컴패니언',
          prompt: '지금 보는 작품 정보 보여줘',
          summary: '재생 중인 작품 정보를 옆 패널로 보여주는 companion 예시',
          pattern: TvSurfacePattern.sidePanel,
          keywords: [
            '지금 보는 작품 정보 보여줘',
            '이거 누구 나와',
            '에피소드 가이드 보여줘',
            '작품 정보',
            '출연진',
          ],
        ),
        presenter: MockCardBriefingPresenter(
          title: '미디어 컴패니언',
          contract: mediaCompanionMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'shopping',
          title: '쇼핑 결정 지원',
          prompt: '상품 비교해줘',
          summary: '가격과 핵심 차이를 카드형으로 비교하는 쇼핑 surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: ['상품 비교해줘', '뭐 사는 게 좋아', '쇼핑 추천 보여줘', '상품 비교', '쇼핑 추천'],
        ),
        presenter: MockCardBriefingPresenter(
          title: '쇼핑 결정 지원',
          contract: shoppingDecisionMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'travel',
          title: '여행 어시스턴트',
          prompt: '여행 일정 보여줘',
          summary: '공항 이동과 예약 정보를 카드형으로 보여주는 여행 surface 예시',
          pattern: TvSurfacePattern.immersive,
          keywords: [
            '여행 일정 보여줘',
            '비행기 정보 보여줘',
            '출국 준비 뭐 남았어',
            '여행 일정',
            '비행기',
            '출국',
          ],
        ),
        presenter: MockCardBriefingPresenter(
          title: '여행 어시스턴트',
          contract: travelAssistantMockContract(),
        ),
      ),
      TemplateDefinition(
        scenario: const TemplateScenario(
          id: 'wellness',
          title: '웰니스 카드',
          prompt: '건강 카드 보여줘',
          summary: '수면과 활동량을 가볍게 보여주는 웰니스 surface 예시',
          pattern: TvSurfacePattern.centerCard,
          keywords: [
            '건강 카드 보여줘',
            '오늘 활동량 알려줘',
            '스트레칭 안내해줘',
            '건강 카드',
            '활동량',
            '스트레칭',
          ],
        ),
        presenter: MockCardBriefingPresenter(
          title: '웰니스 카드',
          contract: wellnessCardMockContract(),
        ),
      ),
    ],
  );
}
