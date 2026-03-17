#include "tv_fetch/cli.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace tv_fetch {

namespace {

bool ContainsControlChars(std::string_view value) {
  return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
    return std::iscntrl(ch) != 0;
  });
}

AppError InvalidArguments(std::string message, std::string hint = {}) {
  return AppError{
      .code = "invalid_arguments",
      .message = std::move(message),
      .hint = std::move(hint),
      .exit_code = 2,
  };
}

bool ParseDouble(std::string_view value, double* out) {
  try {
    std::size_t index = 0;
    const auto parsed = std::stod(std::string(value), &index);
    if (index != value.size()) {
      return false;
    }
    *out = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseInt(std::string_view value, int* out) {
  try {
    std::size_t index = 0;
    const auto parsed = std::stoi(std::string(value), &index);
    if (index != value.size()) {
      return false;
    }
    *out = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

std::variant<OutputFormat, AppError> ParseOutputFormat(std::string_view value) {
  if (value == "json") {
    return OutputFormat::kJson;
  }
  if (value == "pretty") {
    return OutputFormat::kPretty;
  }
  return InvalidArguments(
      "Unsupported format value.",
      "Use --format json or --format pretty.");
}

std::variant<DescribeCommand, AppError> ParseDescribe(
    const std::vector<std::string_view>& args) {
  DescribeCommand command;
  std::size_t index = 0;
  if (index < args.size() && !args[index].starts_with("--")) {
    command.target = std::string(args[index]);
    ++index;
  }

  while (index < args.size()) {
    const auto arg = args[index];
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    return InvalidArguments(
        "Unsupported describe option.",
        "Use tv_fetch describe [weather] [--format json|pretty].");
  }

  return command;
}

std::variant<WeatherCommand, AppError> ParseWeather(
    const std::vector<std::string_view>& args) {
  WeatherCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = WeatherCommand::Source::kMock;
      } else if (value == "open-meteo") {
        command.source = WeatherCommand::Source::kOpenMeteo;
      } else {
        return InvalidArguments(
            "Unsupported weather source.",
            "Use --source mock or --source open-meteo.");
      }
      index += 2;
      continue;
    }
    if (arg == "--city") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --city.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("City contains control characters.");
      }
      command.city = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--district") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --district.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("District contains control characters.");
      }
      command.district = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--latitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --latitude.");
      }
      double latitude = 0.0;
      if (!ParseDouble(args[index + 1], &latitude) || latitude < -90.0 ||
          latitude > 90.0) {
        return InvalidArguments(
            "Latitude must be a number between -90 and 90.");
      }
      command.latitude = latitude;
      index += 2;
      continue;
    }
    if (arg == "--longitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --longitude.");
      }
      double longitude = 0.0;
      if (!ParseDouble(args[index + 1], &longitude) || longitude < -180.0 ||
          longitude > 180.0) {
        return InvalidArguments(
            "Longitude must be a number between -180 and 180.");
      }
      command.longitude = longitude;
      index += 2;
      continue;
    }
    if (arg == "--hours") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --hours.");
      }
      int hours = 0;
      if (!ParseInt(args[index + 1], &hours) || hours < 1 || hours > 24) {
        return InvalidArguments("Hours must be an integer between 1 and 24.");
      }
      command.hours = hours;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported weather option.",
        "Use tv_fetch weather [--source mock|open-meteo] [--city ...] "
        "[--district ...] [--latitude ...] [--longitude ...] [--hours ...] "
        "[--dry-run] [--format json|pretty].");
  }

  return command;
}

nlohmann::json WeatherDescribeDocument() {
  return {
      {"name", "weather"},
      {"description",
       "Fetch and normalize Korea-first weather context for TV composition."},
      {"supports_live", true},
      {"default_source", "mock"},
      {"sources", nlohmann::json::array({"mock", "open-meteo"})},
      {"parameters",
       nlohmann::json::array(
           {{{"name", "--source"},
             {"type", "string"},
             {"required", false},
             {"values", nlohmann::json::array({"mock", "open-meteo"})}},
            {{"name", "--city"},
             {"type", "string"},
             {"required", false},
             {"default", "서울"}},
            {{"name", "--district"},
             {"type", "string"},
             {"required", false},
             {"default", "중구"}},
            {{"name", "--latitude"},
             {"type", "number"},
             {"required", false},
             {"default", 37.5665}},
            {{"name", "--longitude"},
             {"type", "number"},
             {"required", false},
             {"default", 126.9780}},
            {{"name", "--hours"},
             {"type", "integer"},
             {"required", false},
             {"range", nlohmann::json::array({1, 24})},
             {"default", 6}},
            {{"name", "--dry-run"},
             {"type", "boolean"},
             {"required", false},
             {"default", false}},
            {{"name", "--format"},
             {"type", "string"},
             {"required", false},
             {"values", nlohmann::json::array({"json", "pretty"})},
             {"default", "json"}}})},
      {"output_shape",
       {{"domain", "weather"},
        {"source", "mock|open-meteo"},
        {"location", {{"city", "string"}, {"district", "string"}}},
        {"updated_at", "ISO-8601 string"},
        {"headline", "string"},
        {"current",
         {{"temperature_c", "number|null"},
          {"feels_like_c", "number|null"},
          {"condition", "string"},
          {"humidity_pct", "number|null"},
          {"precip_probability_pct", "number|null"}}},
        {"alert",
         {{"level", "string"},
          {"title", "string"},
          {"summary", "string"},
          {"source", "string"},
          {"issued_at", "ISO-8601 string|null"}}},
        {"hourly", "array"},
        {"footer", "string"}}},
  };
}

}  // namespace

std::variant<Command, AppError> ParseCommand(int argc, char** argv) {
  if (argc < 2) {
    return InvalidArguments("Missing command.", "Use tv_fetch describe or tv_fetch weather.");
  }

  const std::string_view command_name(argv[1]);
  std::vector<std::string_view> args;
  args.reserve(static_cast<std::size_t>(argc - 2));
  for (int index = 2; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  if (command_name == "describe") {
    const auto parsed = ParseDescribe(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<DescribeCommand>(parsed)};
  }

  if (command_name == "weather") {
    const auto parsed = ParseWeather(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<WeatherCommand>(parsed)};
  }

  return InvalidArguments(
      "Unknown command.",
      "Use tv_fetch describe or tv_fetch weather.");
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream
      << "tv_fetch\n"
      << "Agent-friendly CLI for fetching normalized TV domain context.\n\n"
      << "Commands:\n"
      << "  describe [weather] [--format json|pretty]\n"
      << "  weather [--source mock|open-meteo] [--city ...] [--district ...]\n"
      << "          [--latitude ...] [--longitude ...] [--hours 1-24]\n"
      << "          [--dry-run] [--format json|pretty]\n";
  return stream.str();
}

std::string_view ToString(OutputFormat format) {
  return format == OutputFormat::kPretty ? "pretty" : "json";
}

nlohmann::json BuildDescribeDocument(const std::optional<std::string>& target) {
  if (!target.has_value()) {
    return {
        {"name", "tv_fetch"},
        {"description",
         "Fetch and normalize TV-ready domain context for downstream A2UI composition."},
        {"commands", nlohmann::json::array({WeatherDescribeDocument()})},
    };
  }

  if (*target == "weather") {
    return WeatherDescribeDocument();
  }

  return {
      {"name", "tv_fetch"},
      {"warning", "Unknown describe target."},
      {"supported_targets", nlohmann::json::array({"weather"})},
  };
}

}  // namespace tv_fetch
