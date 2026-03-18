#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "tv_a2ui_launcher/error.hpp"

namespace tv_a2ui_launcher {

enum class OutputFormat { kJson, kPretty };

struct LaunchCommand {
  std::string app_id = "com.example.openclaw_tv_genui";
  std::string input_file;
  OutputFormat format = OutputFormat::kJson;
  bool dry_run = false;
};

std::variant<LaunchCommand, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
std::string_view ToString(OutputFormat format);

}  // namespace tv_a2ui_launcher
