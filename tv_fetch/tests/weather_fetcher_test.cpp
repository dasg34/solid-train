#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/commute/commute_fetcher.hpp"
#include "tv_fetch/finance/finance_fetcher.hpp"
#include "tv_fetch/json.hpp"
#include "tv_fetch/news/news_fetcher.hpp"
#include "tv_fetch/scenario/scenario_fetcher.hpp"
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
  Assert(root.At("commands").Size() == 15, "describe root command count");

  const auto news = tv_fetch::BuildDescribeDocument(std::string("news"));
  Assert(news.At("name").AsString() == "news", "describe news name");
  const auto finance = tv_fetch::BuildDescribeDocument(std::string("finance"));
  Assert(finance.At("name").AsString() == "finance", "describe finance name");
  const auto commute = tv_fetch::BuildDescribeDocument(std::string("commute"));
  Assert(commute.At("name").AsString() == "commute", "describe commute name");
  const auto sports = tv_fetch::BuildDescribeDocument(std::string("sports"));
  Assert(sports.At("name").AsString() == "sports", "describe sports name");
  const auto schedule =
      tv_fetch::BuildDescribeDocument(std::string("schedule"));
  Assert(schedule.At("name").AsString() == "schedule",
         "describe schedule name");
}

void TestParseNewsQueryCommand() {
  const char* argv[] = {"tv_fetch", "news", "--query", "반도체", "--count", "4"};
  const auto parsed = tv_fetch::ParseCommand(6, const_cast<char**>(argv));
  Assert(std::holds_alternative<tv_fetch::Command>(parsed),
         "news query command should parse");
  const auto& command = std::get<tv_fetch::Command>(parsed);
  Assert(std::holds_alternative<tv_fetch::NewsCommand>(command),
         "parsed command should be news");
  const auto& news = std::get<tv_fetch::NewsCommand>(command);
  Assert(news.query == "반도체", "news query should be preserved");
  Assert(news.count == 4, "news count should be preserved");
}

void TestParseScenarioCommand() {
  const char* argv[] = {
      "tv_fetch", "schedule", "--source", "mock", "--format", "pretty"};
  const auto parsed = tv_fetch::ParseCommand(6, const_cast<char**>(argv));
  Assert(std::holds_alternative<tv_fetch::Command>(parsed),
         "scenario command should parse");
  const auto& command = std::get<tv_fetch::Command>(parsed);
  Assert(std::holds_alternative<tv_fetch::ScenarioCommand>(command),
         "parsed command should be a scenario command");
  const auto& scenario = std::get<tv_fetch::ScenarioCommand>(command);
  Assert(scenario.kind == tv_fetch::ScenarioCommand::Kind::kSchedule,
         "scenario kind should be preserved");
  Assert(scenario.source == "mock", "scenario source should be preserved");
  Assert(scenario.format == tv_fetch::OutputFormat::kPretty,
         "scenario format should be preserved");
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

void TestScenarioMockFixturesLoad() {
  constexpr std::array<std::pair<tv_fetch::ScenarioCommand::Kind,
                                 std::string_view>,
                       10>
      kScenarios = {{
          {tv_fetch::ScenarioCommand::Kind::kDaily, "daily"},
          {tv_fetch::ScenarioCommand::Kind::kEmergency, "emergency"},
          {tv_fetch::ScenarioCommand::Kind::kFamily, "family"},
          {tv_fetch::ScenarioCommand::Kind::kMealDelivery, "meal-delivery"},
          {tv_fetch::ScenarioCommand::Kind::kMedia, "media"},
          {tv_fetch::ScenarioCommand::Kind::kSchedule, "schedule"},
          {tv_fetch::ScenarioCommand::Kind::kShopping, "shopping"},
          {tv_fetch::ScenarioCommand::Kind::kSmartHome, "smart-home"},
          {tv_fetch::ScenarioCommand::Kind::kTravel, "travel"},
          {tv_fetch::ScenarioCommand::Kind::kWellness, "wellness"},
      }};

  for (const auto& [kind, domain] : kScenarios) {
    tv_fetch::ScenarioCommand command;
    command.kind = kind;

    const auto result = tv_fetch::scenario::Execute(command);
    Assert(std::holds_alternative<tv_fetch::JsonValue>(result),
           std::string(domain) + " mock fixture should load");

    const auto& payload = std::get<tv_fetch::JsonValue>(result);
    Assert(payload.At("domain").AsString() == domain,
           std::string(domain) + " mock domain");
    Assert(payload.At("source").AsString() == "mock",
           std::string(domain) + " mock source");
  }
}

}  // namespace

int main() {
  TestDescribeWeather();
  TestDescribeAdditionalDomains();
  TestParseNewsQueryCommand();
  TestParseScenarioCommand();
  TestNormalizeOpenMeteoResponse();
  TestMockFixtureLoads();
  TestAdditionalMockFixturesLoad();
  TestScenarioMockFixturesLoad();
  std::cout << "tv_fetch_tests passed\n";
  return 0;
}
