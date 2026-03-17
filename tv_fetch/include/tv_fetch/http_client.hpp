#pragma once

#include <string>
#include <string_view>
#include <variant>

#include "tv_fetch/error.hpp"

namespace tv_fetch {

std::variant<std::string, AppError> HttpGet(std::string_view url,
                                            long connect_timeout_seconds = 5,
                                            long max_timeout_seconds = 20);

}  // namespace tv_fetch
