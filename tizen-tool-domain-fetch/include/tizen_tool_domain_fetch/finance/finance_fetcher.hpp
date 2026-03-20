#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::finance {

JsonResult Execute(const FinanceCommand& command);
JsonResult LoadMockFinancePayload();

}  // namespace tizen_tool_domain_fetch::finance
