#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::daily {

JsonResult Execute(const DailyCommand& command);
JsonResult LoadMockDailyPayload();

}  // namespace tizen_tool_domain_fetch::daily
