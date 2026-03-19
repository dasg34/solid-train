#include "tizen_tool_viewer_launch/launcher.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>

#include <unistd.h>

#if __has_include(<app_control.h>)
#include <app_control.h>
#define TV_A2UI_LAUNCHER_HAS_APP_CONTROL 1
#else
#define TV_A2UI_LAUNCHER_HAS_APP_CONTROL 0
#endif

namespace tizen_tool_viewer_launch {

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

#if TV_A2UI_LAUNCHER_HAS_APP_CONTROL
constexpr int kColdStartReplayDelayMs = 700;

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

std::optional<AppError> SendLaunchRequest(std::string_view app_id,
                                          std::string_view payload_json) {
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

  ret = app_control_set_app_id(app_control, std::string(app_id).c_str());
  if (ret != APP_CONTROL_ERROR_NONE) {
    destroy();
    return MakeError(
        "app_control_app_id_failed",
        "Failed to set the target application ID.",
        "app_control_set_app_id error=" + std::to_string(ret),
        4);
  }

  ret = app_control_add_extra_data(
      app_control, "json", std::string(payload_json).c_str());
  if (ret != APP_CONTROL_ERROR_NONE) {
    destroy();
    return MakeError(
        "app_control_extra_data_failed",
        "Failed to attach the presentation JSON as extra data.",
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

  return std::nullopt;
}
#endif

}  // namespace

std::variant<PersistedPayload, AppError> PreparePayload(
    const LaunchCommand& command, std::istream& input) {
  if (!command.input_file.empty()) {
    std::ifstream file(command.input_file, std::ios::binary);
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
          "The specified input file does not contain presentation payload data.",
          command.input_file,
          3);
    }

    return PersistedPayload{
        .json = content,
        .source_label = command.input_file,
        .bytes = content.size(),
        .used_stdin = false,
    };
  }

  if (::isatty(STDIN_FILENO) != 0) {
    return MakeError(
        "stdin_required",
        "No piped input was provided.",
        "Pipe presentation JSON into stdin or use --file PATH.",
        2);
  }

  const std::string content = ReadAll(input);
  if (!HasContent(content)) {
    return MakeError(
        "empty_stdin",
        "Standard input did not contain presentation payload data.",
        "Pipe presentation JSON into tizen-tool-viewer-launch or use --file PATH.",
        3);
  }

  return PersistedPayload{
      .json = content,
      .source_label = "stdin",
      .bytes = content.size(),
      .used_stdin = true,
  };
}

std::variant<LaunchReport, AppError> LaunchPayload(
    const LaunchCommand& command, const PersistedPayload& payload) {
  LaunchReport report{
      .app_id = command.app_id,
      .source_label = payload.source_label,
      .bytes = payload.bytes,
      .used_stdin = payload.used_stdin,
      .platform_supported = TV_A2UI_LAUNCHER_HAS_APP_CONTROL != 0,
      .replayed_after_launch = false,
      .replay_delay_ms = 0,
      .message = command.dry_run ? "Dry run: launch request not sent."
                                 : "Launch request sent.",
  };

  if (command.dry_run) {
    return report;
  }

#if TV_A2UI_LAUNCHER_HAS_APP_CONTROL
  if (const auto error =
          SendLaunchRequest(command.app_id, payload.json)) {
    return *error;
  }

  std::this_thread::sleep_for(
      std::chrono::milliseconds(kColdStartReplayDelayMs));
  if (const auto error =
          SendLaunchRequest(command.app_id, payload.json)) {
    return *error;
  }

  report.launched = true;
  report.replayed_after_launch = true;
  report.replay_delay_ms = kColdStartReplayDelayMs;
  report.message =
      "Launch request sent and replayed once for cold-start handoff.";
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
      << indent << "\"source\": \"" << EscapeJson(report.source_label) << "\"," << newline
      << indent << "\"extra_data_key\": \"json\"," << newline
      << indent << "\"bytes\": " << report.bytes << "," << newline
      << indent << "\"used_stdin\": " << (report.used_stdin ? "true" : "false") << "," << newline
      << indent << "\"launched\": " << (report.launched ? "true" : "false") << "," << newline
      << indent << "\"replayed_after_launch\": "
      << (report.replayed_after_launch ? "true" : "false") << "," << newline
      << indent << "\"replay_delay_ms\": " << report.replay_delay_ms << "," << newline
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

}  // namespace tizen_tool_viewer_launch
