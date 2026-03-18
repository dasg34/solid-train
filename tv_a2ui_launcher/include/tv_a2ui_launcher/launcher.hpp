#pragma once

#include <cstddef>
#include <istream>
#include <string>
#include <variant>

#include "tv_a2ui_launcher/cli.hpp"
#include "tv_a2ui_launcher/error.hpp"

namespace tv_a2ui_launcher {

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
  std::string message;
};

std::variant<PersistedPayload, AppError> PreparePayload(
    const LaunchCommand& command, std::istream& input);
std::variant<LaunchReport, AppError> LaunchPayload(
    const LaunchCommand& command, const PersistedPayload& payload);
std::string RenderReport(const LaunchReport& report, OutputFormat format);
std::string RenderError(const AppError& error, OutputFormat format);

}  // namespace tv_a2ui_launcher
