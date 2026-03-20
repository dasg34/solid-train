#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::schedule {

JsonResult Execute(const ScheduleCommand& command);
JsonResult LoadMockSchedulePayload();

}  // namespace tizen_tool_domain_fetch::schedule
