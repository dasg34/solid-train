#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::commute {

JsonResult Execute(const CommuteCommand& command);
JsonResult LoadMockCommutePayload();

}  // namespace tv_fetch::commute
