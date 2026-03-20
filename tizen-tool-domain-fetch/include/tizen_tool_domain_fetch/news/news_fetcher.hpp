#pragma once

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch::news {

JsonResult Execute(const NewsCommand& command);
JsonResult LoadMockNewsPayload();

}  // namespace tizen_tool_domain_fetch::news
