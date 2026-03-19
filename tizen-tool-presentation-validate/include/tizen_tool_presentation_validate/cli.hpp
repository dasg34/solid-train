#pragma once

#include <string>
#include <variant>

#include "tizen_tool_presentation_validate/error.hpp"
#include "tizen_tool_presentation_validate/json.hpp"

namespace tizen_tool_presentation_validate {

enum class OutputFormat { kJson, kPretty };

struct ValidateCommand {
  std::string file;
  OutputFormat format = OutputFormat::kPretty;
};

struct DescribeCommand {
  OutputFormat format = OutputFormat::kPretty;
};

using Command = std::variant<ValidateCommand, DescribeCommand>;

std::variant<Command, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
JsonValue BuildDescribeDocument();

}  // namespace tizen_tool_presentation_validate
