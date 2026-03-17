#pragma once

#include "tv_fetch/cli.hpp"
#include "tv_fetch/json.hpp"

namespace tv_fetch::finance {

JsonResult Execute(const FinanceCommand& command);
JsonResult LoadMockFinancePayload();

}  // namespace tv_fetch::finance
