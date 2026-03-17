#pragma once

#include <string>
#include <variant>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/error.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::weather {

JsonResult Execute(const WeatherCommand& command);

JsonValue NormalizeOpenMeteoResponse(const JsonValue& response,
                                     const std::string& city,
                                     const std::string& district,
                                     int hours);

JsonResult LoadMockWeatherPayload();

}  // namespace tv_fetch::weather
