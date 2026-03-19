#pragma once

#include <string>

namespace tizen_tool_presentation_validate {

struct AppError {
  std::string code;
  std::string message;
  std::string hint;
  int exit_code = 1;
};

}  // namespace tizen_tool_presentation_validate
