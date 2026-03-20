#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::commute {

JsonResult Execute(const CommuteCommand& command);
JsonResult LoadMockCommutePayload();

}  // namespace tizen_tool_domain_fetch::commute
