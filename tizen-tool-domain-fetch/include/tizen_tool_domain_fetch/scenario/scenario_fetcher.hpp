#pragma once

#include <optional>
#include <string_view>

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::scenario {

JsonResult Execute(const ScenarioCommand& command);
JsonValue Describe(const ScenarioCommand::Kind kind);
std::optional<ScenarioCommand::Kind> ParseKind(std::string_view command_name);
std::string_view CommandName(ScenarioCommand::Kind kind);

}  // namespace tizen_tool_domain_fetch::scenario
