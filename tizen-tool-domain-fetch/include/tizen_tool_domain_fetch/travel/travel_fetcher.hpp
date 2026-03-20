#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::travel {

JsonResult Execute(const TravelCommand& command);
JsonResult LoadMockTravelPayload();

}  // namespace tizen_tool_domain_fetch::travel
