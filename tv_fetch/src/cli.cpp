#include "tv_fetch/cli.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
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

JsonValue MakeArray(std::initializer_list<JsonValue> values) {
  JsonValue array = JsonValue::Array();
  for (const auto& value : values) {
    ArrayAppend(array, value);
  }
  return array;
}

JsonValue MakeObject(
    std::initializer_list<std::pair<std::string_view, JsonValue>> entries) {
  JsonValue object = JsonValue::Object();
  for (const auto& [key, value] : entries) {
    ObjectSet(object, key, value);
  }
  return object;
}

JsonValue WeatherDescribeDocument() {
  return MakeObject({
      {"name", JsonValue::String("weather")},
      {"description",
       JsonValue::String(
           "Fetch and normalize Korea-first weather context for TV composition.")},
      {"supports_live", JsonValue::Boolean(true)},
      {"default_source", JsonValue::String("mock")},
      {"sources",
       MakeArray({JsonValue::String("mock"), JsonValue::String("open-meteo")})},
      {"parameters",
       MakeArray({
           MakeObject({
               {"name", JsonValue::String("--source")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"values",
                MakeArray(
                    {JsonValue::String("mock"), JsonValue::String("open-meteo")})},
           }),
           MakeObject({
               {"name", JsonValue::String("--city")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::String("서울")},
           }),
           MakeObject({
               {"name", JsonValue::String("--district")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::String("중구")},
           }),
           MakeObject({
               {"name", JsonValue::String("--latitude")},
               {"type", JsonValue::String("number")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::Double(37.5665)},
           }),
           MakeObject({
               {"name", JsonValue::String("--longitude")},
               {"type", JsonValue::String("number")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::Double(126.9780)},
           }),
           MakeObject({
               {"name", JsonValue::String("--hours")},
               {"type", JsonValue::String("integer")},
               {"required", JsonValue::Boolean(false)},
               {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(24)})},
               {"default", JsonValue::Integer(6)},
           }),
           MakeObject({
               {"name", JsonValue::String("--dry-run")},
               {"type", JsonValue::String("boolean")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::Boolean(false)},
           }),
           MakeObject({
               {"name", JsonValue::String("--format")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"values",
                MakeArray(
                    {JsonValue::String("json"), JsonValue::String("pretty")})},
               {"default", JsonValue::String("json")},
           }),
       })},
      {"output_shape",
       MakeObject({
           {"domain", JsonValue::String("weather")},
           {"source", JsonValue::String("mock|open-meteo")},
           {"location",
            MakeObject({
                {"city", JsonValue::String("string")},
                {"district", JsonValue::String("string")},
            })},
           {"updated_at", JsonValue::String("ISO-8601 string")},
           {"headline", JsonValue::String("string")},
           {"current",
            MakeObject({
                {"temperature_c", JsonValue::String("number|null")},
                {"feels_like_c", JsonValue::String("number|null")},
                {"condition", JsonValue::String("string")},
                {"humidity_pct", JsonValue::String("number|null")},
                {"precip_probability_pct", JsonValue::String("number|null")},
            })},
           {"alert",
            MakeObject({
                {"level", JsonValue::String("string")},
                {"title", JsonValue::String("string")},
                {"summary", JsonValue::String("string")},
                {"source", JsonValue::String("string")},
                {"issued_at", JsonValue::String("ISO-8601 string|null")},
            })},
           {"hourly", JsonValue::String("array")},
           {"footer", JsonValue::String("string")},
       })},
  });
}

}  // namespace

std::variant<Command, AppError> ParseCommand(int argc, char** argv) {
  if (argc < 2) {
    return InvalidArguments("Missing command.",
                            "Use tv_fetch describe or tv_fetch weather.");
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

  return InvalidArguments("Unknown command.",
                          "Use tv_fetch describe or tv_fetch weather.");
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream << "tv_fetch\n"
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

JsonValue BuildDescribeDocument(const std::optional<std::string>& target) {
  if (!target.has_value()) {
    return MakeObject({
        {"name", JsonValue::String("tv_fetch")},
        {"description",
         JsonValue::String(
             "Fetch and normalize TV-ready domain context for downstream A2UI composition.")},
        {"commands", MakeArray({WeatherDescribeDocument()})},
    });
  }

  if (*target == "weather") {
    return WeatherDescribeDocument();
  }

  return MakeObject({
      {"name", JsonValue::String("tv_fetch")},
      {"warning", JsonValue::String("Unknown describe target.")},
      {"supported_targets", MakeArray({JsonValue::String("weather")})},
  });
}

}  // namespace tv_fetch
