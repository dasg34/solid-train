#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::youtube {

enum class TimeFilter {
  kNone,
  kLastHour,
  kLast24Hours,
  kLast7Days,
  kLast30Days,
  kLast365Days,
};

struct PublishedWindow {
  std::string published_after;
  std::string published_before;
};

std::variant<TimeFilter, AppError> TimeFilterFromLegacySp(std::string_view sp);
std::string_view TimeFilterName(TimeFilter filter);
std::string FormatRfc3339Utc(std::time_t timestamp);
std::optional<PublishedWindow> BuildPublishedWindow(TimeFilter filter,
                                                    std::time_t now);
std::string BuildSearchUrl(const YouTubeCommand& command,
                           TimeFilter filter,
                           std::time_t now,
                           std::string_view api_key);
JsonResult NormalizeSearchResponse(const JsonValue& response,
                                   const YouTubeCommand& command,
                                   TimeFilter filter);
JsonResult Execute(const YouTubeCommand& command);

}  // namespace tizen_tool_domain_fetch::youtube
