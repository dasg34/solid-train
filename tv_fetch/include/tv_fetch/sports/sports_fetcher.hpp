#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::sports {

JsonResult Execute(const SportsCommand& command);
JsonResult LoadMockSportsPayload();

}  // namespace tv_fetch::sports
