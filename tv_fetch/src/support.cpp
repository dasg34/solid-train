#include "tv_fetch/support.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace tv_fetch {

namespace {

std::string ProjectRoot() {
#ifdef TV_FETCH_PROJECT_ROOT
  return TV_FETCH_PROJECT_ROOT;
#else
  return ".";
#endif
}

std::vector<std::filesystem::path> CandidateFixturePaths(
    std::string_view file_name) {
  std::vector<std::filesystem::path> candidates;

  if (const char* env_root = std::getenv("TV_FETCH_FIXTURE_ROOT");
      env_root != nullptr && env_root[0] != '\0') {
    const std::filesystem::path root(env_root);
    candidates.push_back(root / file_name);
    candidates.push_back(root / "fixtures" / file_name);
  }

#ifdef TV_FETCH_DEFAULT_FIXTURE_ROOT
  {
    const std::filesystem::path root(TV_FETCH_DEFAULT_FIXTURE_ROOT);
    candidates.push_back(root / file_name);
    candidates.push_back(root / "fixtures" / file_name);
  }
#endif

  const std::filesystem::path source_root(ProjectRoot());
  candidates.push_back(source_root / file_name);
  candidates.push_back(source_root / "fixtures" / file_name);

  return candidates;
}

}  // namespace

std::string CleanText(std::string_view value, std::size_t max_length) {
  std::ostringstream normalized;
  bool last_was_space = false;
  for (unsigned char ch : value) {
    const bool is_space = std::isspace(ch) != 0;
    if (is_space) {
      if (!last_was_space && normalized.tellp() > 0) {
        normalized << ' ';
      }
      last_was_space = true;
      continue;
    }
    normalized << static_cast<char>(ch);
    last_was_space = false;
  }

  std::string text = normalized.str();
  if (text.size() <= max_length) {
    return text;
  }
  return text.substr(0, max_length - 3) + "...";
}

std::string UrlEncode(std::string_view value) {
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (unsigned char ch : value) {
    const bool unreserved =
        (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' ||
        ch == '~';
    if (unreserved) {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(kHex[(ch >> 4) & 0x0F]);
    encoded.push_back(kHex[ch & 0x0F]);
  }
  return encoded;
}

JsonValue MakeObject(
    std::initializer_list<std::pair<std::string_view, JsonValue>> entries) {
  JsonValue object = JsonValue::Object();
  for (const auto& [key, value] : entries) {
    ObjectSet(object, key, value);
  }
  return object;
}

JsonValue MakeArray(std::initializer_list<JsonValue> values) {
  JsonValue array = JsonValue::Array();
  for (const auto& value : values) {
    ArrayAppend(array, value);
  }
  return array;
}

JsonValue CopyOrNull(const JsonValue& value) {
  return value.get() == nullptr ? JsonValue::Null() : value;
}

std::filesystem::path ResolveFixturePath(std::string_view file_name) {
  std::error_code error;
  for (const auto& candidate : CandidateFixturePaths(file_name)) {
    if (std::filesystem::exists(candidate, error) && !error) {
      return candidate;
    }
    error.clear();
  }
  return {};
}

std::string RenderMissingFixtureHint(std::string_view file_name) {
  std::ostringstream hint;
  hint << "Searched: ";
  const auto candidates = CandidateFixturePaths(file_name);
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (index > 0) {
      hint << ", ";
    }
    hint << candidates[index].string();
  }
  return hint.str();
}

JsonResult LoadFixturePayload(std::string_view file_name,
                              std::string_view domain,
                              std::string_view source) {
  const std::filesystem::path path = ResolveFixturePath(file_name);
  if (path.empty()) {
    return AppError{
        .code = "mock_fixture_missing",
        .message = "Failed to locate bundled mock fixture.",
        .hint = RenderMissingFixtureHint(file_name),
        .exit_code = 5,
    };
  }

  std::ifstream handle(path);
  if (!handle.is_open()) {
    return AppError{
        .code = "mock_fixture_missing",
        .message = "Failed to open bundled mock fixture.",
        .hint = path.string(),
        .exit_code = 5,
    };
  }

  std::ostringstream stream;
  stream << handle.rdbuf();
  auto parsed = JsonValue::Parse(stream.str(), "mock_fixture_invalid",
                                 "Bundled mock fixture is not valid JSON.", 5);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }

  JsonValue payload = std::get<JsonValue>(std::move(parsed));
  ObjectSet(payload, "domain", JsonValue::String(domain));
  ObjectSet(payload, "source", JsonValue::String(source));
  return payload;
}

}  // namespace tv_fetch
