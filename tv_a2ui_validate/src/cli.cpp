#include "tv_a2ui_validate/cli.hpp"

#include <sstream>
#include <vector>

namespace tv_a2ui_validate {

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
          "Use tv_a2ui_validate <file> [--format json|pretty].");
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
        "Use tv_a2ui_validate describe [--format json|pretty].");
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
        "Use tv_a2ui_validate <file> or tv_a2ui_validate --stdin.");
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
  stream << "tv_a2ui_validate\n"
         << "A2UI v0.9 NDJSON validator for Samsung Tizen TV.\n\n"
         << "Usage:\n"
         << "  tv_a2ui_validate <file>            Validate an A2UI NDJSON file\n"
         << "  tv_a2ui_validate --stdin           Read from stdin\n"
         << "  tv_a2ui_validate describe          Show validation rules\n\n"
         << "Options:\n"
         << "  --format json|pretty          Output format (default: pretty)\n";
  return stream.str();
}

JsonValue BuildDescribeDocument() {
  return MakeObject({
      {"name", JsonValue::String("tv_a2ui_validate")},
      {"description",
       JsonValue::String(
           "Validates A2UI v0.9 NDJSON against the TV app component catalog.")},
      {"checks",
       MakeArray({
           MakeObject({
               {"rule", JsonValue::String("ndjson_structure")},
               {"description",
                JsonValue::String("Input has exactly 3 lines of valid JSON.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("message_order")},
               {"description",
                JsonValue::String(
                    "Messages are createSurface, updateDataModel, "
                    "updateComponents in order.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("surface_id")},
               {"description",
                JsonValue::String(
                    "surfaceId is consistent across all 3 messages.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("theme")},
               {"description",
                JsonValue::String(
                    "theme.domain and theme.pattern are present and valid.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("root_component")},
               {"description",
                JsonValue::String(
                    "A component with id \"root\" exists.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("component_types")},
               {"description",
                JsonValue::String(
                    "All component types are from the 11 allowed types.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("icon_names")},
               {"description",
                JsonValue::String(
                    "All Icon components use valid Material icon names.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("referential_integrity")},
               {"description",
                JsonValue::String(
                    "All child/children references point to defined IDs.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("data_bindings")},
               {"description",
                JsonValue::String(
                    "All {\"path\": \"/key\"} references exist in "
                    "the dataModel.")},
           }),
           MakeObject({
               {"rule", JsonValue::String("component_count")},
               {"description",
                JsonValue::String(
                    "Total component count does not exceed 40.")},
           }),
       })},
  });
}

}  // namespace tv_a2ui_validate
