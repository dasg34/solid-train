#pragma once

#include <string>
#include <variant>

#include "tv_validate/error.hpp"
#include "tv_validate/json.hpp"

namespace tv_validate {

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

}  // namespace tv_validate
