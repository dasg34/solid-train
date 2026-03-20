#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::emergency {

JsonResult Execute(const EmergencyCommand& command);
JsonResult LoadMockEmergencyPayload();

}  // namespace tizen_tool_domain_fetch::emergency
