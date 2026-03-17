#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::schedule {

JsonResult Execute(const ScheduleCommand& command);
JsonResult LoadMockSchedulePayload();

}  // namespace tv_fetch::schedule
