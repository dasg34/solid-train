enum SurfaceStyle { standard, atmosphericWeather, newsPanel, schedulePanel }

SurfaceStyle resolveSurfaceStyle(String domain) {
  if (domain == 'weather') return SurfaceStyle.atmosphericWeather;
  if (domain == 'news') return SurfaceStyle.newsPanel;
  if (domain == 'schedule') return SurfaceStyle.schedulePanel;
  return SurfaceStyle.standard;
}
