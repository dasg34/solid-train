import 'package:flutter_test/flutter_test.dart';
import 'package:openclaw_tv_genui/core/a2ui/surface_style.dart';

void main() {
  group('resolveSurfaceStyle', () {
    test('weather surfaceId returns atmosphericWeather', () {
      expect(
        resolveSurfaceStyle('weather_main'),
        SurfaceStyle.atmosphericWeather,
      );
    });

    test('news surfaceId returns newsPanel', () {
      expect(resolveSurfaceStyle('news_main'), SurfaceStyle.newsPanel);
    });

    test('schedule surfaceId returns schedulePanel', () {
      expect(
        resolveSurfaceStyle('schedule_main'),
        SurfaceStyle.schedulePanel,
      );
    });

    test('unknown surfaceId returns standard', () {
      expect(resolveSurfaceStyle('sports_main'), SurfaceStyle.standard);
      expect(resolveSurfaceStyle('anything'), SurfaceStyle.standard);
    });
  });
}
