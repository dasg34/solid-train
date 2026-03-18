#include "tv_a2ui_launcher/launcher.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>

#include <unistd.h>

#if __has_include(<app_control.h>)
#include <app_control.h>
#define TV_A2UI_LAUNCHER_HAS_APP_CONTROL 1
#else
#define TV_A2UI_LAUNCHER_HAS_APP_CONTROL 0
#endif

namespace tv_a2ui_launcher {

namespace {

std::string EscapeJson(std::string_view value) {
  std::ostringstream escaped;
  for (const unsigned char ch : value) {
    switch (ch) {
      case '\\':
        escaped << "\\\\";
        break;
      case '"':
        escaped << "\\\"";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (std::iscntrl(ch) != 0) {
          escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<int>(ch) << std::dec;
        } else {
          escaped << static_cast<char>(ch);
        }
        break;
    }
  }
  return escaped.str();
}

AppError MakeError(std::string code,
                   std::string message,
                   std::string hint,
                   int exit_code) {
  return AppError{
      .code = std::move(code),
      .message = std::move(message),
      .hint = std::move(hint),
      .exit_code = exit_code,
  };
}

std::string ReadAll(std::istream& input) {
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool HasContent(std::string_view value) {
  for (const unsigned char ch : value) {
    if (std::isspace(ch) == 0) {
      return true;
    }
  }
  return false;
}

std::filesystem::path DefaultOutputPath() {
  const auto temp_root = std::filesystem::temp_directory_path();
  const auto dir = temp_root / "tv_a2ui_launcher";
  std::filesystem::create_directories(dir);
  const auto timestamp = std::to_string(
      static_cast<long long>(std::time(nullptr)));
  const auto pid = std::to_string(static_cast<long long>(::getpid()));
  return dir / ("payload_" + timestamp + "_" + pid + ".ndjson");
}

#if TV_A2UI_LAUNCHER_HAS_APP_CONTROL
std::optional<std::string> TizenErrorHint(int code) {
  switch (code) {
    case APP_CONTROL_ERROR_APP_NOT_FOUND:
      return "The target application ID was not found on the device.";
    case APP_CONTROL_ERROR_PERMISSION_DENIED:
      return "The caller may be missing appmanager.launch privilege or launch permission.";
    case APP_CONTROL_ERROR_LAUNCH_REJECTED:
      return "The platform rejected the launch request in the current context.";
    case APP_CONTROL_ERROR_INVALID_PARAMETER:
      return "The App Control request contained an invalid parameter.";
    default:
      return std::nullopt;
  }
}
#endif

}  // namespace

std::variant<PersistedPayload, AppError> PreparePayload(
    const LaunchCommand& command, std::istream& input) {
  if (!command.input_file.empty()) {
    const std::filesystem::path path(command.input_file);
    if (!std::filesystem::exists(path)) {
      return MakeError(
          "input_file_missing",
          "The specified input file does not exist.",
          command.input_file,
          3);
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
      return MakeError(
          "input_file_unreadable",
          "The specified input file could not be opened.",
          command.input_file,
          3);
    }

    const std::string content = ReadAll(file);
    if (!HasContent(content)) {
      return MakeError(
          "empty_input",
          "The specified input file does not contain A2UI payload data.",
          command.input_file,
          3);
    }

    return PersistedPayload{
        .file_path = path.string(),
        .bytes = content.size(),
        .used_stdin = false,
    };
  }

  if (::isatty(STDIN_FILENO) != 0) {
    return MakeError(
        "stdin_required",
        "No piped input was provided.",
        "Pipe A2UI NDJSON into stdin or use --file PATH.",
        2);
  }

  const std::string content = ReadAll(input);
  if (!HasContent(content)) {
    return MakeError(
        "empty_stdin",
        "Standard input did not contain A2UI payload data.",
        "Pipe NDJSON into tv_a2ui_launcher or use --file PATH.",
        3);
  }

  const std::filesystem::path output_path =
      command.output_file.empty() ? DefaultOutputPath()
                                  : std::filesystem::path(command.output_file);
  std::filesystem::create_directories(output_path.parent_path());

  std::ofstream output(output_path, std::ios::binary);
  if (!output) {
    return MakeError(
        "output_file_unwritable",
        "Failed to create the persisted payload file.",
        output_path.string(),
        3);
  }
  output << content;
  output.close();

  return PersistedPayload{
      .file_path = output_path.string(),
      .bytes = content.size(),
      .used_stdin = true,
  };
}

std::variant<LaunchReport, AppError> LaunchPayload(
    const LaunchCommand& command, const PersistedPayload& payload) {
  LaunchReport report{
      .app_id = command.app_id,
      .file_path = payload.file_path,
      .bytes = payload.bytes,
      .used_stdin = payload.used_stdin,
      .platform_supported = TV_A2UI_LAUNCHER_HAS_APP_CONTROL != 0,
      .message = command.dry_run ? "Dry run: launch request not sent."
                                 : "Launch request sent.",
  };

  if (command.dry_run) {
    return report;
  }

#if TV_A2UI_LAUNCHER_HAS_APP_CONTROL
  app_control_h app_control = nullptr;
  int ret = app_control_create(&app_control);
  if (ret != APP_CONTROL_ERROR_NONE || app_control == nullptr) {
    return MakeError(
        "app_control_create_failed",
        "Failed to create a Tizen App Control handle.",
        "app_control_create error=" + std::to_string(ret),
        4);
  }

  auto destroy = [&]() { app_control_destroy(app_control); };

  ret = app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);
  if (ret != APP_CONTROL_ERROR_NONE) {
    destroy();
    return MakeError(
        "app_control_operation_failed",
        "Failed to configure the App Control operation.",
        "app_control_set_operation error=" + std::to_string(ret),
        4);
  }

  ret = app_control_set_app_id(app_control, command.app_id.c_str());
  if (ret != APP_CONTROL_ERROR_NONE) {
    destroy();
    return MakeError(
        "app_control_app_id_failed",
        "Failed to set the target application ID.",
        "app_control_set_app_id error=" + std::to_string(ret),
        4);
  }

  ret = app_control_add_extra_data(app_control, "file", payload.file_path.c_str());
  if (ret != APP_CONTROL_ERROR_NONE) {
    destroy();
    return MakeError(
        "app_control_extra_data_failed",
        "Failed to attach the payload file path as extra data.",
        "app_control_add_extra_data error=" + std::to_string(ret),
        4);
  }

  ret = app_control_send_launch_request(app_control, nullptr, nullptr);
  destroy();
  if (ret != APP_CONTROL_ERROR_NONE) {
    return MakeError(
        "app_control_launch_failed",
        "The Tizen launch request failed.",
        TizenErrorHint(ret).value_or(
            "app_control_send_launch_request error=" + std::to_string(ret)),
        4);
  }

  report.launched = true;
  return report;
#else
  return MakeError(
      "tizen_app_control_unavailable",
      "This build does not include Tizen App Control support.",
      "Run on Tizen or build with app_control.h and its native library available.",
      4);
#endif
}

std::string RenderReport(const LaunchReport& report, OutputFormat format) {
  const bool pretty = format == OutputFormat::kPretty;
  const std::string indent = pretty ? "  " : "";
  const std::string newline = pretty ? "\n" : "";

  std::ostringstream out;
  out << "{"
      << newline
      << indent << "\"ok\": true," << newline
      << indent << "\"app_id\": \"" << EscapeJson(report.app_id) << "\"," << newline
      << indent << "\"file\": \"" << EscapeJson(report.file_path) << "\"," << newline
      << indent << "\"bytes\": " << report.bytes << "," << newline
      << indent << "\"used_stdin\": " << (report.used_stdin ? "true" : "false") << "," << newline
      << indent << "\"launched\": " << (report.launched ? "true" : "false") << "," << newline
      << indent << "\"platform_supported\": "
      << (report.platform_supported ? "true" : "false") << "," << newline
      << indent << "\"message\": \"" << EscapeJson(report.message) << "\""
      << newline << "}";
  return out.str();
}

std::string RenderError(const AppError& error, OutputFormat format) {
  const bool pretty = format == OutputFormat::kPretty;
  const std::string indent = pretty ? "  " : "";
  const std::string indent2 = pretty ? "    " : "";
  const std::string newline = pretty ? "\n" : "";

  std::ostringstream out;
  out << "{"
      << newline
      << indent << "\"ok\": false," << newline
      << indent << "\"error\": {" << newline
      << indent2 << "\"code\": \"" << EscapeJson(error.code) << "\"," << newline
      << indent2 << "\"message\": \"" << EscapeJson(error.message) << "\"";
  if (!error.hint.empty()) {
    out << "," << newline
        << indent2 << "\"hint\": \"" << EscapeJson(error.hint) << "\"";
  }
  out << newline
      << indent << "}" << newline
      << "}";
  return out.str();
}

}  // namespace tv_a2ui_launcher
