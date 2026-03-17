#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::daily {

JsonResult Execute(const DailyCommand& command);
JsonResult LoadMockDailyPayload();

}  // namespace tv_fetch::daily
