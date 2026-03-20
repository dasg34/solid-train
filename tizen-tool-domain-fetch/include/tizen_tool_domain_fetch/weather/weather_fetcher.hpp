#pragma once

#include <string>
#include <variant>

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/error.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::weather {

JsonResult Execute(const WeatherCommand& command);

JsonValue NormalizeOpenMeteoResponse(const JsonValue& response,
                                     const std::string& city,
                                     const std::string& district,
                                     int hours);

JsonResult LoadMockWeatherPayload();

}  // namespace tizen_tool_domain_fetch::weather
