#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::news {

JsonResult Execute(const NewsCommand& command);
JsonResult LoadMockNewsPayload();

}  // namespace tv_fetch::news
