#pragma once

#include <string>
#include <utility>

namespace tizen_tool_domain_fetch {

struct AppError {
  AppError() = default;
  AppError(std::string code_value,
           std::string message_value,
           std::string hint_value,
           int exit_code_value = 1)
      : code(std::move(code_value)),
        message(std::move(message_value)),
        hint(std::move(hint_value)),
        exit_code(exit_code_value) {}

  std::string code;
  std::string message;
  std::string hint;
  int exit_code = 1;
};

}  // namespace tizen_tool_domain_fetch
