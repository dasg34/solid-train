#include "tv_a2ui_launcher/cli.hpp"

#include <sstream>

namespace tv_a2ui_launcher {

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
      "Unsupported output format.",
      "Use --format json or --format pretty.");
}

}  // namespace

std::variant<LaunchCommand, AppError> ParseCommand(int argc, char** argv) {
  LaunchCommand command;

  for (int index = 1; index < argc;) {
    const std::string_view arg(argv[index]);
    if (arg == "--app-id") {
      if (index + 1 >= argc) {
        return InvalidArguments("Missing value for --app-id.");
      }
      command.app_id = argv[index + 1];
      index += 2;
      continue;
    }
    if (arg == "--file") {
      if (index + 1 >= argc) {
        return InvalidArguments("Missing value for --file.");
      }
      command.input_file = argv[index + 1];
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= argc) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(argv[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--help" || arg == "-h") {
      return InvalidArguments("Help requested.", RenderHelp());
    }
    return InvalidArguments(
        "Unsupported launcher option.",
        "Use tv_a2ui_launcher [--file PATH] "
        "[--app-id APP_ID] [--dry-run] [--format json|pretty].");
  }

  if (command.app_id.empty()) {
    return InvalidArguments("App ID must not be empty.");
  }

  return command;
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream << "tv_a2ui_launcher\n"
         << "Launch com.example_tv_genui with raw A2UI NDJSON via Tizen App Control.\n\n"
         << "Usage:\n"
         << "  cat /tmp/a2ui.json | tv_a2ui_launcher\n"
         << "  tv_a2ui_launcher --file /tmp/a2ui.json\n"
         << "  tv_a2ui_launcher --file /tmp/a2ui.json --app-id com.example_tv_genui\n"
         << "  tv_a2ui_launcher --file /tmp/a2ui.json --dry-run --format pretty\n\n"
         << "Options:\n"
         << "  --file PATH         Use an existing NDJSON file instead of stdin\n"
         << "  --app-id APP_ID     Target application ID (default: com.example_tv_genui)\n"
         << "  --dry-run           Read the payload but do not send the launch request\n"
         << "  --format FORMAT     Output json or pretty (default: json)\n";
  return stream.str();
}

std::string_view ToString(OutputFormat format) {
  return format == OutputFormat::kPretty ? "pretty" : "json";
}

}  // namespace tv_a2ui_launcher
