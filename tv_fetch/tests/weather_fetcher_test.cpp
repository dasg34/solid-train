#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/weather/weather_fetcher.hpp"

namespace {

std::string ProjectRoot() {
#ifdef TV_FETCH_PROJECT_ROOT
  return TV_FETCH_PROJECT_ROOT;
#else
  return ".";
#endif
}

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    std::exit(1);
  }
}

void TestDescribeWeather() {
  const auto document = tv_fetch::BuildDescribeDocument(std::string("weather"));
  Assert(document.at("name") == "weather", "describe weather name");
  Assert(document.at("parameters").is_array(), "describe weather parameters");
}

void TestNormalizeOpenMeteoResponse() {
  const nlohmann::json sample = {
      {"current",
       {{"time", "2026-03-18T07:00"},
        {"temperature_2m", 8.4},
        {"apparent_temperature", 6.1},
        {"relative_humidity_2m", 61},
        {"weather_code", 3}}},
      {"hourly",
       {{"time",
         nlohmann::json::array({"2026-03-18T07:00", "2026-03-18T08:00"})},
        {"temperature_2m", nlohmann::json::array({8.4, 9.1})},
        {"precipitation_probability", nlohmann::json::array({30, 20})},
        {"weather_code", nlohmann::json::array({3, 2})}}},
  };

  const auto normalized = tv_fetch::weather::NormalizeOpenMeteoResponse(
      sample, "서울", "중구", 2);
  Assert(normalized.at("domain") == "weather", "normalized domain");
  Assert(normalized.at("source") == "open-meteo", "normalized source");
  Assert(normalized.at("current").at("condition") == "흐림",
         "normalized condition");
  Assert(normalized.at("hourly").size() == 2, "normalized hourly size");
}

void TestMockFixtureLoads() {
  const auto result = tv_fetch::weather::LoadMockWeatherPayload();
  Assert(std::holds_alternative<nlohmann::json>(result),
         "mock fixture should load");
  const auto& payload = std::get<nlohmann::json>(result);
  Assert(payload.at("domain") == "weather", "mock domain");
  Assert(payload.at("source") == "mock", "mock source");
}

}  // namespace

int main() {
  TestDescribeWeather();
  TestNormalizeOpenMeteoResponse();
  TestMockFixtureLoads();
  std::cout << "tv_fetch_tests passed\n";
  return 0;
}
