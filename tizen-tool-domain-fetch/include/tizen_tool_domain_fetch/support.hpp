#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "tizen_tool_domain_fetch/json.hpp"

namespace tizen_tool_domain_fetch {

std::string CleanText(std::string_view value, std::size_t max_length = 80);
std::string UrlEncode(std::string_view value);

JsonValue MakeObject(
    std::initializer_list<std::pair<std::string_view, JsonValue>> entries);
JsonValue MakeArray(std::initializer_list<JsonValue> values);
JsonValue CopyOrNull(const JsonValue& value);

std::filesystem::path ResolveFixturePath(std::string_view file_name);
std::string RenderMissingFixtureHint(std::string_view file_name);
JsonResult LoadFixturePayload(std::string_view file_name,
                              std::string_view domain,
                              std::string_view source = "mock");

}  // namespace tizen_tool_domain_fetch
