enum SurfaceStyle { standard, atmosphericWeather, newsPanel, schedulePanel }

SurfaceStyle resolveSurfaceStyle(String surfaceId) {
  if (surfaceId.startsWith('weather')) return SurfaceStyle.atmosphericWeather;
  if (surfaceId.startsWith('news')) return SurfaceStyle.newsPanel;
  if (surfaceId.startsWith('schedule')) return SurfaceStyle.schedulePanel;
  return SurfaceStyle.standard;
}
