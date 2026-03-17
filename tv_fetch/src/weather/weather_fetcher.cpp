#include "tv_fetch/weather/weather_fetcher.hpp"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "tv_fetch/http_client.hpp"

namespace tv_fetch::weather {

namespace {

std::string CleanText(const std::string& value, std::size_t max_length = 80) {
  std::ostringstream normalized;
  bool last_was_space = false;
  for (unsigned char ch : value) {
    const bool is_space = std::isspace(ch) != 0;
    if (is_space) {
      if (!last_was_space && normalized.tellp() > 0) {
        normalized << ' ';
      }
      last_was_space = true;
      continue;
    }
    normalized << static_cast<char>(ch);
    last_was_space = false;
  }

  std::string text = normalized.str();
  if (text.size() <= max_length) {
    return text;
  }
  return text.substr(0, max_length - 3) + "...";
}

std::string FormatTemperature(double value) {
  std::ostringstream stream;
  stream << std::lround(value) << "°";
  return stream.str();
}

std::string OpenMeteoCondition(int code) {
  switch (code) {
    case 0:
      return "맑음";
    case 1:
      return "대체로 맑음";
    case 2:
      return "구름 조금";
    case 3:
      return "흐림";
    case 45:
    case 48:
      return "안개";
    case 51:
    case 53:
    case 55:
      return "이슬비";
    case 61:
    case 63:
    case 65:
      return "비";
    case 71:
    case 73:
    case 75:
    case 77:
      return "눈";
    case 80:
    case 81:
    case 82:
      return "소나기";
    case 95:
    case 96:
    case 99:
      return "뇌우";
    default:
      return "날씨 정보";
  }
}

std::string BuildOpenMeteoUrl(const WeatherCommand& command) {
  std::ostringstream url;
  url << "https://api.open-meteo.com/v1/forecast?"
      << "latitude=" << std::fixed << std::setprecision(4) << command.latitude
      << "&longitude=" << std::fixed << std::setprecision(4)
      << command.longitude << "&timezone=Asia%2FSeoul"
      << "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
         "precipitation,weather_code"
      << "&hourly=temperature_2m,precipitation_probability,weather_code"
      << "&forecast_hours=" << command.hours;
  return url.str();
}

std::string ProjectRoot() {
#ifdef TV_FETCH_PROJECT_ROOT
  return TV_FETCH_PROJECT_ROOT;
#else
  return ".";
#endif
}

std::vector<std::filesystem::path> CandidateFixturePaths(
    std::string_view file_name) {
  std::vector<std::filesystem::path> candidates;

  if (const char* env_root = std::getenv("TV_FETCH_FIXTURE_ROOT");
      env_root != nullptr && env_root[0] != '\0') {
    const std::filesystem::path root(env_root);
    candidates.push_back(root / file_name);
    candidates.push_back(root / "fixtures" / file_name);
  }

#ifdef TV_FETCH_DEFAULT_FIXTURE_ROOT
  {
    const std::filesystem::path root(TV_FETCH_DEFAULT_FIXTURE_ROOT);
    candidates.push_back(root / file_name);
    candidates.push_back(root / "fixtures" / file_name);
  }
#endif

  const std::filesystem::path source_root(ProjectRoot());
  candidates.push_back(source_root / file_name);
  candidates.push_back(source_root / "fixtures" / file_name);

  return candidates;
}

std::filesystem::path ResolveFixturePath(std::string_view file_name) {
  std::error_code error;
  for (const auto& candidate : CandidateFixturePaths(file_name)) {
    if (std::filesystem::exists(candidate, error) && !error) {
      return candidate;
    }
    error.clear();
  }
  return {};
}

std::string RenderMissingFixtureHint(std::string_view file_name) {
  std::ostringstream hint;
  hint << "Searched: ";
  const auto candidates = CandidateFixturePaths(file_name);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (index > 0) {
      hint << ", ";
    }
    hint << candidates[index].string();
  }
  return hint.str();
}

JsonValue MakeObject(
    std::initializer_list<std::pair<std::string_view, JsonValue>> entries) {
  JsonValue object = JsonValue::Object();
  for (const auto& [key, value] : entries) {
    ObjectSet(object, key, value);
  }
  return object;
}

JsonValue CopyOrNull(const JsonValue& value) {
  return value.get() == nullptr ? JsonValue::Null() : value;
}

}  // namespace

JsonResult LoadMockWeatherPayload() {
  const std::filesystem::path path =
      ResolveFixturePath("mock_weather_seoul.json");
  if (path.empty()) {
    return AppError{
        .code = "mock_fixture_missing",
        .message = "Failed to locate bundled mock weather fixture.",
        .hint = RenderMissingFixtureHint("mock_weather_seoul.json"),
        .exit_code = 5,
    };
  }

  std::ifstream handle(path);
  if (!handle.is_open()) {
    return AppError{
        .code = "mock_fixture_missing",
        .message = "Failed to open bundled mock weather fixture.",
        .hint = path.string(),
        .exit_code = 5,
    };
  }

  std::ostringstream stream;
  stream << handle.rdbuf();
  auto parsed =
      JsonValue::Parse(stream.str(), "mock_fixture_invalid",
                       "Bundled mock weather fixture is not valid JSON.", 5);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }

  JsonValue payload = std::get<JsonValue>(std::move(parsed));
  ObjectSet(payload, "domain", JsonValue::String("weather"));
  ObjectSet(payload, "source", JsonValue::String("mock"));
  return payload;
}

JsonValue NormalizeOpenMeteoResponse(const JsonValue& response,
                                     const std::string& city,
                                     const std::string& district,
                                     int hours) {
  const JsonValue current = response.At("current");
  const JsonValue hourly = response.At("hourly");

  const JsonValue hourly_times = hourly.At("time");
  const JsonValue hourly_temps = hourly.At("temperature_2m");
  const JsonValue hourly_precips = hourly.At("precipitation_probability");
  const JsonValue hourly_codes = hourly.At("weather_code");

  JsonValue normalized_hours = JsonValue::Array();
  const std::size_t count = std::min<std::size_t>(
      static_cast<std::size_t>(hours), hourly_times.Size());
  for (std::size_t index = 0; index < count; ++index) {
    const JsonValue hourly_code = hourly_codes.At(index);
    ArrayAppend(
        normalized_hours,
        MakeObject({
            {"time", CopyOrNull(hourly_times.At(index))},
            {"temperature_c", CopyOrNull(hourly_temps.At(index))},
            {"precip_probability_pct", CopyOrNull(hourly_precips.At(index))},
            {"condition",
             JsonValue::String(
                 OpenMeteoCondition(hourly_code.AsInt(-1)))},
        }));
  }

  const double apparent_temperature =
      current.At("apparent_temperature").AsDouble(0.0);
  const std::string condition =
      OpenMeteoCondition(current.At("weather_code").AsInt(-1));
  const JsonValue first_precip_probability =
      normalized_hours.Size() == 0
          ? JsonValue::Null()
          : CopyOrNull(normalized_hours.At(0).At("precip_probability_pct"));

  return MakeObject({
      {"domain", JsonValue::String("weather")},
      {"source", JsonValue::String("open-meteo")},
      {"location",
       MakeObject({
           {"city", JsonValue::String(CleanText(city, 16))},
           {"district", JsonValue::String(CleanText(district, 16))},
       })},
      {"updated_at", JsonValue::String(current.At("time").AsString(""))},
      {"headline",
       JsonValue::String(CleanText(city + " 현재 " + condition + ", 체감 " +
                                       FormatTemperature(apparent_temperature) +
                                       "입니다.",
                                   48))},
      {"current",
       MakeObject({
           {"temperature_c", CopyOrNull(current.At("temperature_2m"))},
           {"feels_like_c", CopyOrNull(current.At("apparent_temperature"))},
           {"condition", JsonValue::String(condition)},
           {"humidity_pct", CopyOrNull(current.At("relative_humidity_2m"))},
           {"precip_probability_pct", first_precip_probability},
       })},
      {"alert",
       MakeObject({
           {"level", JsonValue::String("안내")},
           {"title", JsonValue::String("공식 특보 연동 필요")},
           {"summary",
            JsonValue::String(
                "현재 화면은 Open-Meteo 실황과 예보를 사용합니다. 재난성 특보는 기상청 공식 채널과 별도로 연동하세요.")},
           {"source", JsonValue::String("Open-Meteo")},
           {"issued_at", JsonValue::String(current.At("time").AsString(""))},
       })},
      {"hourly", normalized_hours},
      {"footer",
       JsonValue::String(
           "실황과 예보는 Open-Meteo 기반이며, 특보는 공식 소스를 별도로 연결하는 편이 안전합니다.")},
  });
}

JsonResult Execute(const WeatherCommand& command) {
  if (command.source == WeatherCommand::Source::kMock) {
    if (command.dry_run) {
      const std::filesystem::path fixture_path =
          ResolveFixturePath("mock_weather_seoul.json");
      return MakeObject({
          {"command", JsonValue::String("weather")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           fixture_path.empty() ? JsonValue::Null()
                                : JsonValue::String(fixture_path.string())},
      });
    }
    return LoadMockWeatherPayload();
  }

  const std::string url = BuildOpenMeteoUrl(command);
  if (command.dry_run) {
    return MakeObject({
        {"command", JsonValue::String("weather")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("open-meteo")},
        {"request",
         MakeObject({
             {"url", JsonValue::String(url)},
             {"city", JsonValue::String(command.city)},
             {"district", JsonValue::String(command.district)},
             {"hours", JsonValue::Integer(command.hours)},
         })},
    });
  }

  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    return std::get<AppError>(response);
  }

  auto parsed = JsonValue::Parse(std::get<std::string>(response),
                                 "weather_parse_failed",
                                 "Open-Meteo response could not be parsed.", 6);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }

  return NormalizeOpenMeteoResponse(std::get<JsonValue>(parsed), command.city,
                                    command.district, command.hours);
}

}  // namespace tv_fetch::weather
