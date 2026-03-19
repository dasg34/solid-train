#pragma once

#include <string>

namespace tv_presentation_validate {

struct AppError {
  std::string code;
  std::string message;
  std::string hint;
  int exit_code = 1;
};

}  // namespace tv_presentation_validate
