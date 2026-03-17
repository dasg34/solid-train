#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::emergency {

JsonResult Execute(const EmergencyCommand& command);
JsonResult LoadMockEmergencyPayload();

}  // namespace tv_fetch::emergency
