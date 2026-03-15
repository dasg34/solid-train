# Live Data Notes

- Verified live weather fetch on March 15, 2026 using Open-Meteo for Seoul (`Asia/Seoul`).
- The skill can now emit A2UI directly from live Open-Meteo data with:
  `python3 scripts/generate_weather_a2ui.py --source open-meteo --city 서울 --district 중구 --hours 4`
- Open-Meteo is a practical `mock-first -> live-later` bridge because it does not require an API key for basic forecast access.
- Open-Meteo does not replace official Korean weather alerts. Keep severe-weather or disaster alerts on a separate official source path such as KMA or another approved public feed.
- If the local Python runtime hits SSL or sandbox DNS issues, direct `curl` access may still work. The current script uses `curl` under the hood for the live Open-Meteo branch.
