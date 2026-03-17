#include "tv_fetch/weather/weather_fetcher.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

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

}  // namespace

std::variant<nlohmann::json, AppError> LoadMockWeatherPayload() {
  const std::string path = ProjectRoot() + "/fixtures/mock_weather_seoul.json";
  std::ifstream handle(path);
  if (!handle.is_open()) {
    return AppError{
        .code = "mock_fixture_missing",
        .message = "Failed to open bundled mock weather fixture.",
        .hint = path,
        .exit_code = 5,
    };
  }

  try {
    nlohmann::json payload = nlohmann::json::parse(handle);
    payload["domain"] = "weather";
    payload["source"] = "mock";
    return payload;
  } catch (const std::exception& error) {
    return AppError{
        .code = "mock_fixture_invalid",
        .message = "Bundled mock weather fixture is not valid JSON.",
        .hint = error.what(),
        .exit_code = 5,
    };
  }
}

nlohmann::json NormalizeOpenMeteoResponse(const nlohmann::json& response,
                                         const std::string& city,
                                         const std::string& district,
                                         int hours) {
  const auto current = response.at("current");
  const auto hourly = response.at("hourly");

  const auto hourly_times = hourly.value("time", std::vector<std::string>{});
  const auto hourly_temps =
      hourly.value("temperature_2m", std::vector<double>{});
  const auto hourly_precips =
      hourly.value("precipitation_probability", std::vector<double>{});
  const auto hourly_codes = hourly.value("weather_code", std::vector<int>{});

  nlohmann::json normalized_hours = nlohmann::json::array();
  const std::size_t count =
      std::min<std::size_t>(static_cast<std::size_t>(hours), hourly_times.size());
  for (std::size_t index = 0; index < count; ++index) {
    normalized_hours.push_back({
        {"time", hourly_times[index]},
        {"temperature_c",
         index < hourly_temps.size() ? nlohmann::json(hourly_temps[index])
                                     : nlohmann::json(nullptr)},
        {"precip_probability_pct",
         index < hourly_precips.size() ? nlohmann::json(hourly_precips[index])
                                       : nlohmann::json(nullptr)},
        {"condition",
         OpenMeteoCondition(index < hourly_codes.size() ? hourly_codes[index]
                                                        : -1)},
    });
  }

  const double apparent_temperature =
      current.value("apparent_temperature", 0.0);
  const std::string condition =
      OpenMeteoCondition(current.value("weather_code", -1));
  const nlohmann::json first_precip_probability =
      normalized_hours.empty()
          ? nlohmann::json(nullptr)
          : normalized_hours.front().value("precip_probability_pct",
                                           nlohmann::json(nullptr));

  return {
      {"domain", "weather"},
      {"source", "open-meteo"},
      {"location",
       {{"city", CleanText(city, 16)}, {"district", CleanText(district, 16)}}},
      {"updated_at", current.value("time", "")},
      {"headline",
       CleanText(city + " 현재 " + condition + ", 체감 " +
                     FormatTemperature(apparent_temperature) + "입니다.",
                 48)},
      {"current",
       {{"temperature_c", current.value("temperature_2m", nlohmann::json(nullptr))},
        {"feels_like_c",
         current.value("apparent_temperature", nlohmann::json(nullptr))},
        {"condition", condition},
        {"humidity_pct",
         current.value("relative_humidity_2m", nlohmann::json(nullptr))},
        {"precip_probability_pct", first_precip_probability}}},
      {"alert",
       {{"level", "안내"},
        {"title", "공식 특보 연동 필요"},
        {"summary",
         "현재 화면은 Open-Meteo 실황과 예보를 사용합니다. 재난성 특보는 기상청 공식 채널과 별도로 연동하세요."},
        {"source", "Open-Meteo"},
        {"issued_at", current.value("time", "")}}},
      {"hourly", normalized_hours},
      {"footer",
       "실황과 예보는 Open-Meteo 기반이며, 특보는 공식 소스를 별도로 연결하는 편이 안전합니다."},
  };
}

std::variant<nlohmann::json, AppError> Execute(const WeatherCommand& command) {
  if (command.source == WeatherCommand::Source::kMock) {
    if (command.dry_run) {
      return nlohmann::json{
          {"command", "weather"},
          {"mode", "dry-run"},
          {"source", "mock"},
          {"fixture_path", ProjectRoot() + "/fixtures/mock_weather_seoul.json"},
      };
    }
    return LoadMockWeatherPayload();
  }

  const std::string url = BuildOpenMeteoUrl(command);
  if (command.dry_run) {
    return nlohmann::json{
        {"command", "weather"},
        {"mode", "dry-run"},
        {"source", "open-meteo"},
        {"request",
         {{"url", url},
          {"city", command.city},
          {"district", command.district},
          {"hours", command.hours}}},
    };
  }

  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    return std::get<AppError>(response);
  }

  try {
    const auto parsed = nlohmann::json::parse(std::get<std::string>(response));
    return NormalizeOpenMeteoResponse(parsed, command.city, command.district,
                                      command.hours);
  } catch (const std::exception& error) {
    return AppError{
        .code = "weather_parse_failed",
        .message = "Open-Meteo response could not be parsed.",
        .hint = error.what(),
        .exit_code = 6,
    };
  }
}

}  // namespace tv_fetch::weather
