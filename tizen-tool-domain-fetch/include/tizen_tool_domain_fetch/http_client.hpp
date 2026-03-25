#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "tizen_tool_domain_fetch/error.hpp"

namespace tizen_tool_domain_fetch {

struct HttpResponse {
  HttpResponse() = default;
  HttpResponse(long status_code_value, std::string body_value)
      : status_code(status_code_value), body(std::move(body_value)) {}

  long status_code = 0;
  std::string body;
};

std::variant<std::string, AppError> HttpGet(std::string_view url,
                                            long connect_timeout_seconds = 5,
                                            long max_timeout_seconds = 20);

std::variant<HttpResponse, AppError> HttpGetDetailed(
    std::string_view url,
    long connect_timeout_seconds = 5,
    long max_timeout_seconds = 20);

std::variant<std::string, AppError> HttpPostForm(
    std::string_view url,
    std::string_view form_body,
    std::string_view accept = "application/json",
    long connect_timeout_seconds = 5,
    long max_timeout_seconds = 20);

}  // namespace tizen_tool_domain_fetch
