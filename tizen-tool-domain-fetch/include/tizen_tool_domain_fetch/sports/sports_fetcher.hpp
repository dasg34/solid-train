#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::sports {

JsonResult Execute(const SportsCommand& command);
JsonResult LoadMockSportsPayload();

}  // namespace tizen_tool_domain_fetch::sports
