#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/commute/commute_fetcher.hpp"
#include "tv_fetch/finance/finance_fetcher.hpp"
#include "tv_fetch/json.hpp"
#include "tv_fetch/news/news_fetcher.hpp"
#include "tv_fetch/sports/sports_fetcher.hpp"
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

void TestDescribeAdditionalDomains() {
  const auto root = tv_fetch::BuildDescribeDocument(std::nullopt);
  Assert(root.At("commands").IsArray(), "describe root commands");
  Assert(root.At("commands").Size() == 5, "describe root command count");

  const auto news = tv_fetch::BuildDescribeDocument(std::string("news"));
  Assert(news.At("name").AsString() == "news", "describe news name");
  const auto finance = tv_fetch::BuildDescribeDocument(std::string("finance"));
  Assert(finance.At("name").AsString() == "finance", "describe finance name");
  const auto commute = tv_fetch::BuildDescribeDocument(std::string("commute"));
  Assert(commute.At("name").AsString() == "commute", "describe commute name");
  const auto sports = tv_fetch::BuildDescribeDocument(std::string("sports"));
  Assert(sports.At("name").AsString() == "sports", "describe sports name");
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

void TestAdditionalMockFixturesLoad() {
  const auto news = tv_fetch::news::LoadMockNewsPayload();
  Assert(std::holds_alternative<tv_fetch::JsonValue>(news),
         "news mock fixture should load");
  Assert(std::get<tv_fetch::JsonValue>(news).At("domain").AsString() == "news",
         "news mock domain");

  const auto finance = tv_fetch::finance::LoadMockFinancePayload();
  Assert(std::holds_alternative<tv_fetch::JsonValue>(finance),
         "finance mock fixture should load");
  Assert(std::get<tv_fetch::JsonValue>(finance).At("domain").AsString() ==
             "finance",
         "finance mock domain");

  const auto commute = tv_fetch::commute::LoadMockCommutePayload();
  Assert(std::holds_alternative<tv_fetch::JsonValue>(commute),
         "commute mock fixture should load");
  Assert(std::get<tv_fetch::JsonValue>(commute).At("domain").AsString() ==
             "commute",
         "commute mock domain");

  const auto sports = tv_fetch::sports::LoadMockSportsPayload();
  Assert(std::holds_alternative<tv_fetch::JsonValue>(sports),
         "sports mock fixture should load");
  Assert(std::get<tv_fetch::JsonValue>(sports).At("domain").AsString() ==
             "sports",
         "sports mock domain");
}

}  // namespace

int main() {
  TestDescribeWeather();
  TestDescribeAdditionalDomains();
  TestNormalizeOpenMeteoResponse();
  TestMockFixtureLoads();
  TestAdditionalMockFixturesLoad();
  std::cout << "tv_fetch_tests passed\n";
  return 0;
}
