#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "tizen_tool_viewer_launch/error.hpp"

namespace tizen_tool_viewer_launch {

enum class OutputFormat { kJson, kPretty };

struct LaunchCommand {
  std::string app_id = "org.tizen.tizen-tool-viewer";
  std::string input_file;
  OutputFormat format = OutputFormat::kJson;
  bool dry_run = false;
};

std::variant<LaunchCommand, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
std::string_view ToString(OutputFormat format);

}  // namespace tizen_tool_viewer_launch
