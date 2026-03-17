#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::travel {

JsonResult Execute(const TravelCommand& command);
JsonResult LoadMockTravelPayload();

}  // namespace tv_fetch::travel
