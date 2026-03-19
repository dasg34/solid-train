#pragma once

#include <cstddef>
#include <istream>
#include <string>
#include <variant>

#include "tizen_tool_viewer_launch/cli.hpp"
#include "tizen_tool_viewer_launch/error.hpp"

namespace tizen_tool_viewer_launch {

struct PersistedPayload {
  std::string json;
  std::string source_label;
  std::size_t bytes = 0;
  bool used_stdin = false;
};

struct LaunchReport {
  std::string app_id;
  std::string source_label;
  std::size_t bytes = 0;
  bool used_stdin = false;
  bool launched = false;
  bool platform_supported = false;
  bool replayed_after_launch = false;
  int replay_delay_ms = 0;
  std::string message;
};

std::variant<PersistedPayload, AppError> PreparePayload(
    const LaunchCommand& command, std::istream& input);
std::variant<LaunchReport, AppError> LaunchPayload(
    const LaunchCommand& command, const PersistedPayload& payload);
std::string RenderReport(const LaunchReport& report, OutputFormat format);
std::string RenderError(const AppError& error, OutputFormat format);

}  // namespace tizen_tool_viewer_launch
