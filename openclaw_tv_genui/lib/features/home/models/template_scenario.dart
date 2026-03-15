enum TvSurfacePattern { immersive, sidePanel, centerCard }

extension TvSurfacePatternView on TvSurfacePattern {
  String get label {
    return switch (this) {
      TvSurfacePattern.immersive => '풀 캔버스',
      TvSurfacePattern.sidePanel => '사이드 패널',
      TvSurfacePattern.centerCard => '센터 카드',
    };
  }

  String get detail {
    return switch (this) {
      TvSurfacePattern.immersive => '큰 화면의 대부분을 활용하는 몰입형 surface',
      TvSurfacePattern.sidePanel => '기존 화면을 남겨두고 정보만 덧붙이는 패널형 surface',
      TvSurfacePattern.centerCard => '짧은 확인과 일정 요약에 맞는 집중형 카드',
    };
  }
}

class TemplateScenario {
  const TemplateScenario({
    required this.id,
    required this.title,
    required this.prompt,
    required this.summary,
    required this.pattern,
    required this.keywords,
  });

  final String id;
  final String title;
  final String prompt;
  final String summary;
  final TvSurfacePattern pattern;
  final List<String> keywords;
}
