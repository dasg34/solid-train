enum TvSurfacePattern {
  immersive,
  sidePanel,
  centerCard,
  topBanner,
  bottomRibbon,
}

extension TvSurfacePatternView on TvSurfacePattern {
  String get label {
    return switch (this) {
      TvSurfacePattern.immersive => '풀 캔버스',
      TvSurfacePattern.sidePanel => '사이드 패널',
      TvSurfacePattern.centerCard => '센터 카드',
      TvSurfacePattern.topBanner => '상단 배너',
      TvSurfacePattern.bottomRibbon => '하단 리본',
    };
  }

  String get detail {
    return switch (this) {
      TvSurfacePattern.immersive => '큰 화면의 대부분을 활용하는 몰입형 surface',
      TvSurfacePattern.sidePanel => '기존 화면을 남겨두고 정보만 덧붙이는 패널형 surface',
      TvSurfacePattern.centerCard => '짧은 확인과 일정 요약에 맞는 집중형 카드',
      TvSurfacePattern.topBanner => '상단에 길게 붙어 빠르게 읽히는 배너형 surface',
      TvSurfacePattern.bottomRibbon => '하단에 길게 깔리는 리본형 보조 surface',
    };
  }
}

class ScenarioEntry {
  const ScenarioEntry({
    required this.id,
    required this.title,
    required this.summary,
    required this.domain,
    required this.surfaceId,
    required this.pattern,
  });

  final String id;
  final String title;
  final String summary;
  final String domain;
  final String surfaceId;
  final TvSurfacePattern pattern;
}

const scenarioCatalog = <ScenarioEntry>[
  ScenarioEntry(
    id: 'weather',
    title: '날씨 브리핑',
    summary: '현재 날씨, 시간별 예보, 체감 온도',
    domain: 'weather',
    surfaceId: 'weather',
    pattern: TvSurfacePattern.immersive,
  ),
  ScenarioEntry(
    id: 'weather_today',
    title: '오늘 날씨 카드',
    summary: '실시간 날씨 snapshot을 data-first 경로로 렌더링',
    domain: 'weather',
    surfaceId: 'weather_today',
    pattern: TvSurfacePattern.immersive,
  ),
  ScenarioEntry(
    id: 'news',
    title: '뉴스 브리핑',
    summary: '주요 헤드라인과 속보',
    domain: 'news',
    surfaceId: 'news',
    pattern: TvSurfacePattern.sidePanel,
  ),
  ScenarioEntry(
    id: 'schedule',
    title: '일정 보드',
    summary: '오늘·내일 일정, 다음 약속',
    domain: 'schedule',
    surfaceId: 'schedule',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'daily',
    title: '데일리 브리핑',
    summary: '날씨, 뉴스, 일정 종합',
    domain: 'daily',
    surfaceId: 'daily',
    pattern: TvSurfacePattern.immersive,
  ),
  ScenarioEntry(
    id: 'sports',
    title: '스포츠 브리핑',
    summary: '실시간 스코어, 다음 경기, 순위',
    domain: 'sports',
    surfaceId: 'sports',
    pattern: TvSurfacePattern.bottomRibbon,
  ),
  ScenarioEntry(
    id: 'finance',
    title: '금융 스냅샷',
    summary: '관심 종목, 환율, 시장 동향',
    domain: 'finance',
    surfaceId: 'finance',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'finance_focus',
    title: '종목 포커스 카드',
    summary: 'LLM이 고른 핵심 수치와 추세를 단일 카드로 정리',
    domain: 'finance',
    surfaceId: 'finance_focus',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'commute',
    title: '출퇴근 안내',
    summary: '이동 시간, 교통 상황, 출발 추천',
    domain: 'commute',
    surfaceId: 'commute',
    pattern: TvSurfacePattern.sidePanel,
  ),
  ScenarioEntry(
    id: 'smart_home',
    title: '스마트홈 요약',
    summary: '문, 조명, 온도, 공기질, 카메라',
    domain: 'smart_home',
    surfaceId: 'smart_home',
    pattern: TvSurfacePattern.sidePanel,
  ),
  ScenarioEntry(
    id: 'emergency',
    title: '긴급 알림',
    summary: '기상특보, 기기 경고, 보안 이벤트',
    domain: 'emergency',
    surfaceId: 'emergency',
    pattern: TvSurfacePattern.topBanner,
  ),
  ScenarioEntry(
    id: 'family',
    title: '가족 메시지 보드',
    summary: '공유 알림, 학교 공지, 생일',
    domain: 'family',
    surfaceId: 'family',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'delivery',
    title: '배달 현황',
    summary: '주문 진행, 도착 예정, 배달원 위치',
    domain: 'delivery',
    surfaceId: 'delivery',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'media',
    title: '미디어 컴패니언',
    summary: '출연진, 에피소드, 사운드트랙',
    domain: 'media',
    surfaceId: 'media',
    pattern: TvSurfacePattern.sidePanel,
  ),
  ScenarioEntry(
    id: 'shopping',
    title: '쇼핑 비교',
    summary: '상품 비교, 추천, 리뷰 요약',
    domain: 'shopping',
    surfaceId: 'shopping',
    pattern: TvSurfacePattern.centerCard,
  ),
  ScenarioEntry(
    id: 'travel',
    title: '여행 어시스턴트',
    summary: '탑승 카운트다운, 게이트, 호텔, 일정',
    domain: 'travel',
    surfaceId: 'travel',
    pattern: TvSurfacePattern.immersive,
  ),
  ScenarioEntry(
    id: 'wellness',
    title: '웰니스 카드',
    summary: '활동 목표, 수면 요약, 스트레칭',
    domain: 'wellness',
    surfaceId: 'wellness',
    pattern: TvSurfacePattern.centerCard,
  ),
];
