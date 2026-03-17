#include <cstdlib>
#include <iostream>
#include <string>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"
#include "tv_fetch/weather/weather_fetcher.hpp"

namespace {

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    std::exit(1);
  }
}

void TestDescribeWeather() {
  const auto document = tv_fetch::BuildDescribeDocument(std::string("weather"));
  Assert(document.At("name").AsString() == "weather", "describe weather name");
  Assert(document.At("parameters").IsArray(), "describe weather parameters");
}

void TestNormalizeOpenMeteoResponse() {
  const auto sample = tv_fetch::JsonValue::Parse(
      R"({
        "current": {
          "time": "2026-03-18T07:00",
          "temperature_2m": 8.4,
          "apparent_temperature": 6.1,
          "relative_humidity_2m": 61,
          "weather_code": 3
        },
        "hourly": {
          "time": ["2026-03-18T07:00", "2026-03-18T08:00"],
          "temperature_2m": [8.4, 9.1],
          "precipitation_probability": [30, 20],
          "weather_code": [3, 2]
        }
      })",
      "test_parse_failed", "Sample weather JSON should parse.", 1);

  Assert(std::holds_alternative<tv_fetch::JsonValue>(sample),
         "sample weather JSON should parse");

  const auto normalized = tv_fetch::weather::NormalizeOpenMeteoResponse(
      std::get<tv_fetch::JsonValue>(sample), "서울", "중구", 2);
  Assert(normalized.At("domain").AsString() == "weather", "normalized domain");
  Assert(normalized.At("source").AsString() == "open-meteo",
         "normalized source");
  Assert(normalized.At("current").At("condition").AsString() == "흐림",
         "normalized condition");
  Assert(normalized.At("hourly").Size() == 2, "normalized hourly size");
}

void TestMockFixtureLoads() {
  const auto result = tv_fetch::weather::LoadMockWeatherPayload();
  Assert(std::holds_alternative<tv_fetch::JsonValue>(result),
         "mock fixture should load");
  const auto& payload = std::get<tv_fetch::JsonValue>(result);
  Assert(payload.At("domain").AsString() == "weather", "mock domain");
  Assert(payload.At("source").AsString() == "mock", "mock source");
}

}  // namespace

int main() {
  TestDescribeWeather();
  TestNormalizeOpenMeteoResponse();
  TestMockFixtureLoads();
  std::cout << "tv_fetch_tests passed\n";
  return 0;
}
