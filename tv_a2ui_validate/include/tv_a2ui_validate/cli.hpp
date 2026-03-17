#pragma once

#include <string>
#include <variant>

#include "tv_a2ui_validate/error.hpp"
#include "tv_a2ui_validate/json.hpp"

namespace tv_a2ui_validate {

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

}  // namespace tv_a2ui_validate
