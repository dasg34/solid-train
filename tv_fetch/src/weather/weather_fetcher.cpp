#include "tv_fetch/weather/weather_fetcher.hpp"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
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
      << "latitude=" << std::fixed << std::setprecision(4)
      << command.latitude.value()
      << "&longitude=" << std::fixed << std::setprecision(4)
      << command.longitude.value() << "&timezone=Asia%2FSeoul"
      << "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
         "precipitation,weather_code"
      << "&hourly=temperature_2m,precipitation_probability,weather_code"
      << "&forecast_hours=" << command.hours;
  return url.str();
}

std::string NormalizeToken(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isspace(ch) != 0) {
      continue;
    }
    normalized.push_back(static_cast<char>(std::tolower(ch)));
  }
  return normalized;
}

bool ContainsToken(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return false;
  }
  const std::string normalized_haystack = NormalizeToken(haystack);
  const std::string normalized_needle = NormalizeToken(needle);
  return !normalized_needle.empty() &&
         normalized_haystack.find(normalized_needle) != std::string::npos;
}

std::string UrlEncode(std::string_view value) {
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (unsigned char ch : value) {
    const bool unreserved =
        (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' ||
        ch == '~';
    if (unreserved) {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHex[(ch >> 4) & 0x0F]);
    encoded.push_back(kHex[ch & 0x0F]);
  }
  return encoded;
}

std::string BuildGeocodingQuery(const WeatherCommand& command) {
  if (!command.city.empty() && !command.district.empty()) {
    return command.city + " " + command.district;
  }
  if (!command.city.empty()) {
    return command.city;
  }
  return command.district;
}

std::string BuildGeocodingUrl(const WeatherCommand& command) {
  const std::string query = BuildGeocodingQuery(command);
  std::ostringstream url;
  url << "https://nominatim.openstreetmap.org/search?"
      << "q=" << UrlEncode(query)
      << "&format=jsonv2"
      << "&limit=5"
      << "&accept-language=ko";
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

struct ResolvedLocation {
  double latitude = 0.0;
  double longitude = 0.0;
  std::string city;
  std::string district;
  std::string geocoding_query;
  std::string geocoding_url;
};

int ScoreGeocodingResult(const JsonValue& result, const WeatherCommand& command) {
  int score = 0;
  const std::vector<std::string> fields = {
      result.At("name").AsString(""),
      result.At("display_name").AsString(""),
  };

  if (!command.district.empty()) {
    for (const auto& field : fields) {
      if (NormalizeToken(field) == NormalizeToken(command.district)) {
        score += 10;
      } else if (ContainsToken(field, command.district)) {
        score += 4;
      }
    }
  }

  if (!command.city.empty()) {
    for (const auto& field : fields) {
      if (NormalizeToken(field) == NormalizeToken(command.city)) {
        score += 6;
      } else if (ContainsToken(field, command.city)) {
        score += 2;
      }
    }
  }

  return score;
}

std::variant<double, AppError> ParseCoordinate(std::string_view value,
                                               std::string_view field,
                                               const std::string& hint) {
  try {
    return std::stod(std::string(value));
  } catch (...) {
    return AppError{
        .code = "geocode_invalid_response",
        .message = "Geocoding response did not contain usable coordinates.",
        .hint = std::string(field) + ": " + hint,
        .exit_code = 7,
    };
  }
}

std::variant<ResolvedLocation, AppError> ResolveLocation(
    const WeatherCommand& command) {
  if (command.latitude.has_value() && command.longitude.has_value()) {
    return ResolvedLocation{
        .latitude = *command.latitude,
        .longitude = *command.longitude,
        .city = command.city,
        .district = command.district,
    };
  }

  const std::string geocoding_query = BuildGeocodingQuery(command);
  const std::string geocoding_url = BuildGeocodingUrl(command);
  const auto geocoding_response = HttpGet(geocoding_url);
  if (std::holds_alternative<AppError>(geocoding_response)) {
    AppError error = std::get<AppError>(geocoding_response);
    error.code = "geocode_request_failed";
    error.message = "Geocoding request failed.";
    error.hint = geocoding_url + " | " + error.hint;
    return error;
  }

  auto parsed = JsonValue::Parse(std::get<std::string>(geocoding_response),
                                 "geocode_parse_failed",
                                 "Geocoding response could not be parsed.", 7);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }

  const JsonValue results = std::get<JsonValue>(parsed);
  if (!results.IsArray() || results.Size() == 0) {
    return AppError{
        .code = "geocode_no_match",
        .message = "No matching location was found for the requested city/district.",
        .hint = geocoding_query,
        .exit_code = 7,
    };
  }

  JsonValue best = results.At(0);
  int best_score = ScoreGeocodingResult(best, command);
  for (std::size_t index = 1; index < results.Size(); ++index) {
    const JsonValue candidate = results.At(index);
    const int candidate_score = ScoreGeocodingResult(candidate, command);
    if (candidate_score > best_score) {
      best = candidate;
      best_score = candidate_score;
    }
  }

  const std::string latitude_text = best.At("lat").AsString("");
  const std::string longitude_text = best.At("lon").AsString("");
  if (latitude_text.empty() || longitude_text.empty()) {
    return AppError{
        .code = "geocode_invalid_response",
        .message = "Geocoding response did not contain usable coordinates.",
        .hint = geocoding_url,
        .exit_code = 7,
    };
  }

  const auto latitude = ParseCoordinate(latitude_text, "lat", geocoding_url);
  if (std::holds_alternative<AppError>(latitude)) {
    return std::get<AppError>(latitude);
  }
  const auto longitude =
      ParseCoordinate(longitude_text, "lon", geocoding_url);
  if (std::holds_alternative<AppError>(longitude)) {
    return std::get<AppError>(longitude);
  }

  return ResolvedLocation{
      .latitude = std::get<double>(latitude),
      .longitude = std::get<double>(longitude),
      .city = command.city.empty()
                  ? best.At("name").AsString("")
                  : command.city,
      .district = command.district.empty() ? best.At("name").AsString("") : command.district,
      .geocoding_query = geocoding_query,
      .geocoding_url = geocoding_url,
  };
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

  if (command.dry_run) {
    JsonValue request = MakeObject({
        {"city", JsonValue::String(command.city)},
        {"district", JsonValue::String(command.district)},
        {"hours", JsonValue::Integer(command.hours)},
    });
    if (command.latitude.has_value() && command.longitude.has_value()) {
      ObjectSet(request, "latitude", JsonValue::Double(*command.latitude));
      ObjectSet(request, "longitude", JsonValue::Double(*command.longitude));

      WeatherCommand resolved_command = command;
      const std::string url = BuildOpenMeteoUrl(resolved_command);
      ObjectSet(request, "url", JsonValue::String(url));
    } else {
      ObjectSet(
          request, "geocoding",
          MakeObject({
              {"query", JsonValue::String(BuildGeocodingQuery(command))},
              {"url", JsonValue::String(BuildGeocodingUrl(command))},
          }));
    }

    return MakeObject({
        {"command", JsonValue::String("weather")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("open-meteo")},
        {"request", std::move(request)},
    });
  }

  const auto resolved_location = ResolveLocation(command);
  if (std::holds_alternative<AppError>(resolved_location)) {
    return std::get<AppError>(resolved_location);
  }
  const auto& location = std::get<ResolvedLocation>(resolved_location);

  WeatherCommand resolved_command = command;
  resolved_command.latitude = location.latitude;
  resolved_command.longitude = location.longitude;

  const std::string url = BuildOpenMeteoUrl(resolved_command);

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

  return NormalizeOpenMeteoResponse(std::get<JsonValue>(parsed), location.city,
                                    location.district, command.hours);
}

}  // namespace tv_fetch::weather
