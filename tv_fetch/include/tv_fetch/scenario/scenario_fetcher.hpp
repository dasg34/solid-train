#pragma once

#include <optional>
#include <string_view>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::scenario {

JsonResult Execute(const ScenarioCommand& command);
JsonValue Describe(const ScenarioCommand::Kind kind);
std::optional<ScenarioCommand::Kind> ParseKind(std::string_view command_name);
std::string_view CommandName(ScenarioCommand::Kind kind);

}  // namespace tv_fetch::scenario
