#include "tv_presentation_validate/cli.hpp"

#include <sstream>
#include <vector>

namespace tv_presentation_validate {

namespace {

AppError InvalidArguments(std::string message, std::string hint = {}) {
  return AppError{
      .code = "invalid_arguments",
      .message = std::move(message),
      .hint = std::move(hint),
      .exit_code = 2,
  };
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

std::variant<ValidateCommand, AppError> ParseValidate(
    const std::vector<std::string_view>& args) {
  ValidateCommand command;

  for (std::size_t index = 0; index < args.size();) {
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
    if (arg == "--stdin" || arg == "-") {
      command.file.clear();
      ++index;
      continue;
    }
    if (arg.starts_with("--")) {
      return InvalidArguments(
          "Unsupported option.",
          "Use tv_presentation_validate <file> [--format json|pretty].");
    }
    command.file = std::string(arg);
    ++index;
  }

  return command;
}

std::variant<DescribeCommand, AppError> ParseDescribe(
    const std::vector<std::string_view>& args) {
  DescribeCommand command;

  for (std::size_t index = 0; index < args.size();) {
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
        "Use tv_presentation_validate describe [--format json|pretty].");
  }

  return command;
}

JsonValue MakeObject(
    std::initializer_list<std::pair<std::string_view, JsonValue>> entries) {
  JsonValue object = JsonValue::Object();
  for (const auto& [key, value] : entries) {
    ObjectSet(object, key, value);
  }
  return object;
}

JsonValue MakeArray(std::initializer_list<JsonValue> values) {
  JsonValue array = JsonValue::Array();
  for (const auto& value : values) {
    ArrayAppend(array, value);
  }
  return array;
}

}  // namespace

std::variant<Command, AppError> ParseCommand(int argc, char** argv) {
  if (argc < 2) {
    return InvalidArguments(
        "Missing file path.",
        "Use tv_presentation_validate <file> or tv_presentation_validate --stdin.");
  }

  const std::string_view first(argv[1]);
  std::vector<std::string_view> args;
  args.reserve(static_cast<std::size_t>(argc - 2));
  for (int index = 2; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  if (first == "describe") {
    const auto parsed = ParseDescribe(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<DescribeCommand>(parsed)};
  }

  args.insert(args.begin(), first);
  const auto parsed = ParseValidate(args);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }
  return Command{std::get<ValidateCommand>(parsed)};
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream << "tv_presentation_validate\n"
         << "Presentation JSON validator for Samsung Tizen TV.\n\n"
         << "Usage:\n"
         << "  tv_presentation_validate <file>     Validate a presentation JSON file\n"
         << "  tv_presentation_validate --stdin    Read from stdin\n"
         << "  tv_presentation_validate describe   Show validation rules\n\n"
         << "Options:\n"
         << "  --format json|pretty               Output format (default: pretty)\n";
  return stream.str();
}

JsonValue BuildDescribeDocument() {
  return MakeObject({
      {"name", JsonValue::String("tv_presentation_validate")},
      {"description",
       JsonValue::String(
           "Validates semantic TV presentation JSON before the Flutter app "
           "converts it into deterministic A2UI.")},
      {"checks",
       MakeArray({
           MakeObject({
               {"rule", JsonValue::String("payload_structure")},
               {"description",
                JsonValue::String(
                    "Input is one raw JSON object, not Markdown, json labels, "
                    "or legacy A2UI NDJSON.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("required_fields")},
               {"description",
                JsonValue::String(
                    "surfaceId, theme, title, and hero required fields are "
                    "present.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("theme")},
               {"description",
                JsonValue::String(
                    "theme.domain and theme.pattern are present and valid.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("metrics")},
               {"description",
                JsonValue::String(
                    "metrics and facts, if present, are arrays of objects with "
                    "label and value strings.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("chart")},
               {"description",
                JsonValue::String(
                    "chart.kind is line or bar, values are numeric, and labels "
                    "match values length when present.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("alert")},
               {"description",
                JsonValue::String(
                    "alert has title and summary strings when present.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("forbidden_fields")},
               {"description",
                JsonValue::String(
                    "footer and legacy raw A2UI envelope fields are rejected.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("tv_density")},
               {"description",
                JsonValue::String(
                    "Warns when metrics/facts exceed 4 items or chart points "
                    "fall outside the 2-8 TV readability range.")},
           }),
       })},
  });
}

}  // namespace tv_presentation_validate
