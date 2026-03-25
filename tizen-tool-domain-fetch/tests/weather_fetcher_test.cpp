#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/commute/commute_fetcher.hpp"
#include "tizen_tool_domain_fetch/daily/daily_fetcher.hpp"
#include "tizen_tool_domain_fetch/emergency/emergency_fetcher.hpp"
#include "tizen_tool_domain_fetch/finance/finance_fetcher.hpp"
#include "tizen_tool_domain_fetch/json.hpp"
#include "tizen_tool_domain_fetch/news/news_fetcher.hpp"
#include "tizen_tool_domain_fetch/scenario/scenario_fetcher.hpp"
#include "tizen_tool_domain_fetch/schedule/schedule_fetcher.hpp"
#include "tizen_tool_domain_fetch/sports/sports_fetcher.hpp"
#include "tizen_tool_domain_fetch/travel/travel_fetcher.hpp"
#include "tizen_tool_domain_fetch/weather/weather_fetcher.hpp"
#include "tizen_tool_domain_fetch/youtube/youtube_fetcher.hpp"

namespace {

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    std::exit(1);
  }
}

void TestDescribeWeather() {
  const auto document = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("weather"));
  Assert(document.At("name").AsString() == "weather", "describe weather name");
  Assert(document.At("parameters").IsArray(), "describe weather parameters");
}

void TestDescribeAdditionalDomains() {
  const auto root = tizen_tool_domain_fetch::BuildDescribeDocument(std::nullopt);
  Assert(root.At("commands").IsArray(), "describe root commands");
  Assert(root.At("commands").Size() == 16, "describe root command count");

  const auto news = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("news"));
  Assert(news.At("name").AsString() == "news", "describe news name");
  const auto youtube =
      tizen_tool_domain_fetch::BuildDescribeDocument(std::string("youtube"));
  Assert(youtube.At("name").AsString() == "youtube", "describe youtube name");
  Assert(youtube.At("compatibility").At("time_zone").AsString() == "Asia/Seoul",
         "describe youtube time zone");
  const auto finance = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("finance"));
  Assert(finance.At("name").AsString() == "finance", "describe finance name");
  const auto commute = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("commute"));
  Assert(commute.At("name").AsString() == "commute", "describe commute name");
  const auto sports = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("sports"));
  Assert(sports.At("name").AsString() == "sports", "describe sports name");
  const auto schedule =
      tizen_tool_domain_fetch::BuildDescribeDocument(std::string("schedule"));
  Assert(schedule.At("name").AsString() == "schedule",
         "describe schedule name");
}

void TestParseNewsQueryCommand() {
  const char* argv[] = {"tizen-tool-domain-fetch", "news", "--query", "반도체", "--count", "4"};
  const auto parsed = tizen_tool_domain_fetch::ParseCommand(6, const_cast<char**>(argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::Command>(parsed),
         "news query command should parse");
  const auto& command = std::get<tizen_tool_domain_fetch::Command>(parsed);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::NewsCommand>(command),
         "parsed command should be news");
  const auto& news = std::get<tizen_tool_domain_fetch::NewsCommand>(command);
  Assert(news.query == "반도체", "news query should be preserved");
  Assert(news.count == 4, "news count should be preserved");
  Assert(news.source == tizen_tool_domain_fetch::NewsCommand::Source::kGoogleNewsRss,
         "news query should auto-switch to google news search");
}

void TestNewsDefaultsAreExplicitInDescribeAndParse() {
  const char* argv[] = {"tizen-tool-domain-fetch", "news"};
  const auto parsed = tizen_tool_domain_fetch::ParseCommand(2, const_cast<char**>(argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::Command>(parsed),
         "plain news command should parse");
  const auto& command = std::get<tizen_tool_domain_fetch::Command>(parsed);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::NewsCommand>(command),
         "plain news command type");
  Assert(std::get<tizen_tool_domain_fetch::NewsCommand>(command).source ==
             tizen_tool_domain_fetch::NewsCommand::Source::kYonhapRss,
         "plain news should default to latest yonhap feed");

  const auto describe = tizen_tool_domain_fetch::BuildDescribeDocument(std::string("news"));
  Assert(describe.At("mode_selection").At("default_source").AsString() ==
             "yonhap-rss",
         "news describe should expose default source");
  Assert(describe.At("mode_selection")
                 .At("query_requires_search_source")
                 .AsBoolean(false) == true,
         "news describe should expose query search rule");
}

void TestParseYouTubeCommand() {
  const char* argv[] = {
      "tizen-tool-domain-fetch", "youtube", "--query", "아이유", "--sp",
      "today",                   "--count", "7"};
  const auto parsed = tizen_tool_domain_fetch::ParseCommand(8, const_cast<char**>(argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::Command>(parsed),
         "youtube command should parse");
  const auto& command = std::get<tizen_tool_domain_fetch::Command>(parsed);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::YouTubeCommand>(command),
         "parsed command should be youtube");
  const auto& youtube = std::get<tizen_tool_domain_fetch::YouTubeCommand>(command);
  Assert(youtube.query == "아이유", "youtube query should be preserved");
  Assert(youtube.sp == "today", "youtube sp should be preserved");
  Assert(youtube.count == 7, "youtube count should be preserved");
}

void TestYouTubeTimeFilterHelpers() {
  const auto none = tizen_tool_domain_fetch::youtube::TimeFilterFromLegacySp("");
  Assert(std::holds_alternative<tizen_tool_domain_fetch::youtube::TimeFilter>(none),
         "empty sp should map to none");
  Assert(std::get<tizen_tool_domain_fetch::youtube::TimeFilter>(none) ==
             tizen_tool_domain_fetch::youtube::TimeFilter::kNone,
         "empty sp should map to none filter");

  setenv("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_WEEK", "LEGACY_WEEK_TOKEN", 1);
  const auto configured =
      tizen_tool_domain_fetch::youtube::TimeFilterFromLegacySp("LEGACY_WEEK_TOKEN");
  Assert(std::holds_alternative<tizen_tool_domain_fetch::youtube::TimeFilter>(configured),
         "configured sp token should parse");
  Assert(std::get<tizen_tool_domain_fetch::youtube::TimeFilter>(configured) ==
             tizen_tool_domain_fetch::youtube::TimeFilter::kLast7Days,
         "configured sp token should map to week");
  unsetenv("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_WEEK");

  const auto invalid =
      tizen_tool_domain_fetch::youtube::TimeFilterFromLegacySp("UNKNOWN_TOKEN");
  Assert(std::holds_alternative<tizen_tool_domain_fetch::AppError>(invalid),
         "unknown sp should fail");
}

void TestYouTubeWindowAndUrlBuilders() {
  const std::time_t now = 1704067200;
  const auto window = tizen_tool_domain_fetch::youtube::BuildPublishedWindow(
      tizen_tool_domain_fetch::youtube::TimeFilter::kLast30Days, now);
  Assert(window.has_value(), "month filter should produce a published window");
  Assert(window->published_after == "2023-12-02T00:00:00Z",
         "month window start should be 30 days earlier");
  Assert(window->published_before == "2024-01-01T00:00:00Z",
         "month window end should be now");

  tizen_tool_domain_fetch::YouTubeCommand command;
  command.query = "K-pop live";
  command.count = 5;
  const std::string url = tizen_tool_domain_fetch::youtube::BuildSearchUrl(
      command, tizen_tool_domain_fetch::youtube::TimeFilter::kLast30Days, now,
      "key123");
  Assert(url.find("q=K-pop%20live") != std::string::npos, "query should be url encoded");
  Assert(url.find("maxResults=5") != std::string::npos, "maxResults should be present");
  Assert(url.find("publishedAfter=2023-12-02T00%3A00%3A00Z") != std::string::npos,
         "publishedAfter should be encoded");
  Assert(url.find("publishedBefore=2024-01-01T00%3A00%3A00Z") != std::string::npos,
         "publishedBefore should be encoded");
  Assert(url.find("key=key123") != std::string::npos, "api key should be appended");
}

void TestNormalizeYouTubeResponse() {
  const auto sample = tizen_tool_domain_fetch::JsonValue::Parse(
      R"({
        "pageInfo": {
          "totalResults": 2
        },
        "items": [
          {
            "id": {
              "videoId": "video-1"
            },
            "snippet": {
              "title": "Lead Video",
              "channelTitle": "Alpha Channel",
              "publishedAt": "2026-03-25T01:00:00Z",
              "thumbnails": {
                "high": {
                  "url": "https://img.example/high.jpg"
                }
              }
            }
          },
          {
            "id": {
              "videoId": "video-2"
            },
            "snippet": {
              "title": "Fallback Video",
              "channelTitle": "Beta Channel",
              "publishedAt": "2026-03-25T02:00:00Z",
              "thumbnails": {
                "default": {
                  "url": "https://img.example/default.jpg"
                }
              }
            }
          }
        ]
      })",
      "test_parse_failed", "Sample youtube JSON should parse.", 1);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(sample),
         "sample youtube JSON should parse");

  tizen_tool_domain_fetch::YouTubeCommand command;
  command.query = "아이유";
  command.sp = "today";
  command.count = 2;
  const auto normalized = tizen_tool_domain_fetch::youtube::NormalizeSearchResponse(
      std::get<tizen_tool_domain_fetch::JsonValue>(sample), command,
      tizen_tool_domain_fetch::youtube::TimeFilter::kLast24Hours);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(normalized),
         "youtube search response should normalize");

  const auto& payload = std::get<tizen_tool_domain_fetch::JsonValue>(normalized);
  Assert(payload.At("domain").AsString() == "youtube", "youtube domain");
  Assert(payload.At("source").AsString() == "youtube-data-api-v3",
         "youtube source");
  Assert(payload.At("timeFilter").AsString() == "last-24-hours",
         "youtube time filter");
  Assert(payload.At("videos").Size() == 2, "youtube results size");
  Assert(payload.At("videos").At(0).At("videoId").AsString() == "video-1",
         "youtube first id");
  Assert(payload.At("videos").At(1).At("thumbnail").AsString() ==
             "https://img.example/default.jpg",
         "youtube thumbnail fallback");
  Assert(payload.At("totalResults").AsInt() == 2, "youtube total results");
}

void TestParseScheduleCommand() {
  const char* argv[] = {
      "tizen-tool-domain-fetch", "schedule", "--source", "mock", "--format", "pretty"};
  const auto parsed = tizen_tool_domain_fetch::ParseCommand(6, const_cast<char**>(argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::Command>(parsed),
         "schedule command should parse");
  const auto& command = std::get<tizen_tool_domain_fetch::Command>(parsed);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::ScheduleCommand>(command),
         "parsed command should be schedule");
  const auto& schedule = std::get<tizen_tool_domain_fetch::ScheduleCommand>(command);
  Assert(schedule.source == tizen_tool_domain_fetch::ScheduleCommand::Source::kMock,
         "schedule source should be preserved");
  Assert(schedule.format == tizen_tool_domain_fetch::OutputFormat::kPretty,
         "schedule format should be preserved");
}

void TestLiveDefaultsAndMockOnlyScenarios() {
  const char* weather_argv[] = {"tizen-tool-domain-fetch", "weather"};
  const auto weather_parsed =
      tizen_tool_domain_fetch::ParseCommand(2, const_cast<char**>(weather_argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::AppError>(weather_parsed),
         "weather should require an explicit location");

  const char* weather_city_argv[] = {"tizen-tool-domain-fetch", "weather", "--city", "서울"};
  const auto weather_city_parsed =
      tizen_tool_domain_fetch::ParseCommand(4, const_cast<char**>(weather_city_argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::Command>(weather_city_parsed),
         "weather with city should parse");
  const auto& weather_command = std::get<tizen_tool_domain_fetch::Command>(weather_city_parsed);
  Assert(std::holds_alternative<tizen_tool_domain_fetch::WeatherCommand>(weather_command),
         "weather with city command type");
  Assert(std::get<tizen_tool_domain_fetch::WeatherCommand>(weather_command).source ==
             tizen_tool_domain_fetch::WeatherCommand::Source::kOpenMeteo,
         "weather with city should still default to live open-meteo");

  const char* commute_argv[] = {"tizen-tool-domain-fetch", "commute"};
  const auto commute_parsed =
      tizen_tool_domain_fetch::ParseCommand(2, const_cast<char**>(commute_argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::AppError>(commute_parsed),
         "commute should require both origin and destination");

  const auto commute_describe =
      tizen_tool_domain_fetch::BuildDescribeDocument(std::string("commute"));
  Assert(commute_describe.At("input_contract")
                 .At("requires_origin")
                 .AsBoolean(false),
         "commute describe should expose origin requirement");
  Assert(commute_describe.At("input_contract")
                 .At("requires_destination")
                 .AsBoolean(false),
         "commute describe should expose destination requirement");

  const char* family_argv[] = {"tizen-tool-domain-fetch", "family"};
  const auto family_parsed =
      tizen_tool_domain_fetch::ParseCommand(2, const_cast<char**>(family_argv));
  Assert(std::holds_alternative<tizen_tool_domain_fetch::AppError>(family_parsed),
         "mock-only scenario should require explicit mock source");
}

void TestNormalizeOpenMeteoResponse() {
  const auto sample = tizen_tool_domain_fetch::JsonValue::Parse(
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

  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(sample),
         "sample weather JSON should parse");

  const auto normalized = tizen_tool_domain_fetch::weather::NormalizeOpenMeteoResponse(
      std::get<tizen_tool_domain_fetch::JsonValue>(sample), "서울", "중구", 2);
  Assert(normalized.At("domain").AsString() == "weather", "normalized domain");
  Assert(normalized.At("source").AsString() == "open-meteo",
         "normalized source");
  Assert(normalized.At("current").At("condition").AsString() == "흐림",
         "normalized condition");
  Assert(normalized.At("hourly").Size() == 2, "normalized hourly size");
}

void TestMockFixtureLoads() {
  const auto result = tizen_tool_domain_fetch::weather::LoadMockWeatherPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(result),
         "mock fixture should load");
  const auto& payload = std::get<tizen_tool_domain_fetch::JsonValue>(result);
  Assert(payload.At("domain").AsString() == "weather", "mock domain");
  Assert(payload.At("source").AsString() == "mock", "mock source");
}

void TestAdditionalMockFixturesLoad() {
  const auto news = tizen_tool_domain_fetch::news::LoadMockNewsPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(news),
         "news mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(news).At("domain").AsString() == "news",
         "news mock domain");

  const auto finance = tizen_tool_domain_fetch::finance::LoadMockFinancePayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(finance),
         "finance mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(finance).At("domain").AsString() ==
             "finance",
         "finance mock domain");

  const auto commute = tizen_tool_domain_fetch::commute::LoadMockCommutePayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(commute),
         "commute mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(commute).At("domain").AsString() ==
             "commute",
         "commute mock domain");

  const auto sports = tizen_tool_domain_fetch::sports::LoadMockSportsPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(sports),
         "sports mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(sports).At("domain").AsString() ==
             "sports",
         "sports mock domain");

  const auto schedule = tizen_tool_domain_fetch::schedule::LoadMockSchedulePayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(schedule),
         "schedule mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(schedule).At("domain").AsString() ==
             "schedule",
         "schedule mock domain");

  const auto travel = tizen_tool_domain_fetch::travel::LoadMockTravelPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(travel),
         "travel mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(travel).At("domain").AsString() ==
             "travel",
         "travel mock domain");

  const auto emergency = tizen_tool_domain_fetch::emergency::LoadMockEmergencyPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(emergency),
         "emergency mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(emergency).At("domain").AsString() ==
             "emergency",
         "emergency mock domain");

  const auto daily = tizen_tool_domain_fetch::daily::LoadMockDailyPayload();
  Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(daily),
         "daily mock fixture should load");
  Assert(std::get<tizen_tool_domain_fetch::JsonValue>(daily).At("domain").AsString() ==
             "daily",
         "daily mock domain");
}

void TestScenarioMockFixturesLoad() {
  constexpr std::array<std::pair<tizen_tool_domain_fetch::ScenarioCommand::Kind,
                                 std::string_view>,
                       6>
      kScenarios = {{
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kFamily, "family"},
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kMealDelivery, "meal-delivery"},
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kMedia, "media"},
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kShopping, "shopping"},
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kSmartHome, "smart-home"},
          {tizen_tool_domain_fetch::ScenarioCommand::Kind::kWellness, "wellness"},
      }};

  for (const auto& [kind, domain] : kScenarios) {
    tizen_tool_domain_fetch::ScenarioCommand command;
    command.kind = kind;
    command.source = "mock";

    const auto result = tizen_tool_domain_fetch::scenario::Execute(command);
    Assert(std::holds_alternative<tizen_tool_domain_fetch::JsonValue>(result),
           std::string(domain) + " mock fixture should load");

    const auto& payload = std::get<tizen_tool_domain_fetch::JsonValue>(result);
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
  TestNewsDefaultsAreExplicitInDescribeAndParse();
  TestParseYouTubeCommand();
  TestYouTubeTimeFilterHelpers();
  TestYouTubeWindowAndUrlBuilders();
  TestNormalizeYouTubeResponse();
  TestParseScheduleCommand();
  TestLiveDefaultsAndMockOnlyScenarios();
  TestNormalizeOpenMeteoResponse();
  TestMockFixtureLoads();
  TestAdditionalMockFixturesLoad();
  TestScenarioMockFixturesLoad();
  std::cout << "tizen-tool-domain-fetch tests passed\n";
  return 0;
}
