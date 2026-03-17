#pragma once

#include <string>
#include <variant>

#include <nlohmann/json.hpp>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/error.hpp"

namespace tv_fetch::weather {

std::variant<nlohmann::json, AppError> Execute(const WeatherCommand& command);

nlohmann::json NormalizeOpenMeteoResponse(const nlohmann::json& response,
                                         const std::string& city,
                                         const std::string& district,
                                         int hours);

std::variant<nlohmann::json, AppError> LoadMockWeatherPayload();

}  // namespace tv_fetch::weather
