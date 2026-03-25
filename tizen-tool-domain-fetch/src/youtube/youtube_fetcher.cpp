#include "tizen_tool_domain_fetch/youtube/youtube_fetcher.hpp"

#include <array>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::youtube {

namespace {

constexpr std::string_view kSearchUrl =
    "https://www.googleapis.com/youtube/v3/search";
constexpr std::string_view kPrimaryApiKeyEnv =
    "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_API_KEY";
constexpr std::string_view kFallbackApiKeyEnv = "YOUTUBE_DATA_API_KEY";
constexpr std::string_view kTimeZoneId = "Asia/Seoul";
constexpr std::time_t kKstOffsetSeconds = 9 * 60 * 60;

struct FilterMapping {
  TimeFilter filter;
  std::string_view alias;
  std::string_view token_env_name;
  std::string_view name;
  std::time_t window_seconds;
};

constexpr std::array<FilterMapping, 5> kFilterMappings = {{
    {TimeFilter::kLastHour,
     "last-hour",
     "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_LAST_HOUR",
     "last-hour",
     60 * 60},
    {TimeFilter::kLast24Hours,
     "today",
     "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_TODAY",
     "last-24-hours",
     24 * 60 * 60},
    {TimeFilter::kLast7Days,
     "week",
     "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_WEEK",
     "last-7-days",
     7 * 24 * 60 * 60},
    {TimeFilter::kLast30Days,
     "month",
     "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_MONTH",
     "last-30-days",
     30 * 24 * 60 * 60},
    {TimeFilter::kLast365Days,
     "year",
     "TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_YEAR",
     "last-365-days",
     365 * 24 * 60 * 60},
}};

struct ApiKeyConfig {
  std::string env_name;
  std::string api_key;
};

AppError MakeYouTubeError(std::string code,
                          std::string message,
                          std::string hint,
                          int exit_code = 6) {
  return AppError{std::move(code), std::move(message), std::move(hint),
                  exit_code};
}

std::optional<TimeFilter> MatchConfiguredToken(std::string_view sp) {
  for (const auto& mapping : kFilterMappings) {
    if (sp == mapping.alias) {
      return mapping.filter;
    }
    const char* configured = std::getenv(std::string(mapping.token_env_name).c_str());
    if (configured != nullptr && configured[0] != '\0' && sp == configured) {
      return mapping.filter;
    }
  }
  return std::nullopt;
}

std::optional<ApiKeyConfig> ResolveApiKeyConfig() {
  const std::array<std::string_view, 2> env_names = {
      kPrimaryApiKeyEnv,
      kFallbackApiKeyEnv,
  };
  for (const auto env_name : env_names) {
    const char* value = std::getenv(std::string(env_name).c_str());
    if (value != nullptr && value[0] != '\0') {
      return ApiKeyConfig{std::string(env_name), std::string(value)};
    }
  }
  return std::nullopt;
}

std::tm GmTime(std::time_t timestamp) {
  const std::tm* utc = std::gmtime(&timestamp);
  if (utc == nullptr) {
    throw MakeYouTubeError("youtube_time_failed",
                           "Failed to resolve UTC timestamp.",
                           "std::gmtime returned null.");
  }
  return *utc;
}

std::time_t UtcToKst(std::time_t utc) {
  return utc + kKstOffsetSeconds;
}

std::time_t KstToUtc(std::time_t kst) {
  return kst - kKstOffsetSeconds;
}

std::string JsonString(const JsonValue& object,
                       std::string_view key,
                       std::string_view fallback = {}) {
  if (!object.IsObject() || !ObjectHasKey(object, key)) {
    return std::string(fallback);
  }
  return object.At(key).AsString(fallback);
}

std::string ExtractThumbnailUrl(const JsonValue& snippet) {
  const JsonValue thumbnails = snippet.At("thumbnails");
  if (!thumbnails.IsObject()) {
    return {};
  }

  constexpr std::array<std::string_view, 3> kThumbnailSizes = {
      "high",
      "medium",
      "default",
  };
  for (const auto key : kThumbnailSizes) {
    const JsonValue candidate = thumbnails.At(key);
    const std::string url = JsonString(candidate, "url");
    if (!url.empty()) {
      return url;
    }
  }
  return {};
}

std::string BuildSearchUrlWithoutKey(const YouTubeCommand& command,
                                     TimeFilter filter,
                                     std::time_t now) {
  std::ostringstream stream;
  stream << kSearchUrl
         << "?part=snippet"
         << "&type=video"
         << "&maxResults=" << command.count
         << "&regionCode=KR"
         << "&relevanceLanguage=ko"
         << "&safeSearch=moderate"
         << "&q=" << UrlEncode(command.query);

  if (const auto window = BuildPublishedWindow(filter, now); window.has_value()) {
    stream << "&publishedAfter=" << UrlEncode(window->published_after)
           << "&publishedBefore=" << UrlEncode(window->published_before);
  }

  return stream.str();
}

AppError InvalidSpError(std::string_view sp) {
  std::ostringstream hint;
  hint << "Supported aliases: ";
  for (std::size_t index = 0; index < kFilterMappings.size(); ++index) {
    if (index > 0) {
      hint << ", ";
    }
    hint << kFilterMappings[index].alias;
  }
  hint << ". Runtime token envs: ";
  for (std::size_t index = 0; index < kFilterMappings.size(); ++index) {
    if (index > 0) {
      hint << ", ";
    }
    hint << kFilterMappings[index].token_env_name;
  }
  hint << ". Received: " << sp;
  return MakeYouTubeError("youtube_invalid_sp",
                          "Unsupported YouTube legacy sp token.",
                          hint.str(),
                          2);
}

std::optional<AppError> ApiErrorFromResponse(const JsonValue& document,
                                             long status_code) {
  const JsonValue error = document.At("error");
  if (!error.IsObject()) {
    return std::nullopt;
  }

  const JsonValue errors = error.At("errors");
  std::string reason;
  if (errors.IsArray() && errors.Size() > 0) {
    reason = JsonString(errors.At(0), "reason");
  }

  std::ostringstream hint;
  hint << "HTTP " << status_code;
  if (!reason.empty()) {
    hint << " | " << reason;
  }

  return MakeYouTubeError("youtube_api_error",
                          JsonString(error, "message",
                                     "YouTube Data API request failed."),
                          hint.str());
}

}  // namespace

std::variant<TimeFilter, AppError> TimeFilterFromLegacySp(std::string_view sp) {
  if (sp.empty()) {
    return TimeFilter::kNone;
  }
  if (const auto matched = MatchConfiguredToken(sp); matched.has_value()) {
    return *matched;
  }
  return InvalidSpError(sp);
}

std::string_view TimeFilterName(TimeFilter filter) {
  if (filter == TimeFilter::kNone) {
    return "none";
  }
  for (const auto& mapping : kFilterMappings) {
    if (mapping.filter == filter) {
      return mapping.name;
    }
  }
  return "unknown";
}

std::string FormatRfc3339Utc(std::time_t timestamp) {
  std::ostringstream stream;
  const std::tm utc = GmTime(timestamp);
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

std::optional<PublishedWindow> BuildPublishedWindow(TimeFilter filter,
                                                    std::time_t now) {
  if (filter == TimeFilter::kNone) {
    return std::nullopt;
  }

  for (const auto& mapping : kFilterMappings) {
    if (mapping.filter != filter) {
      continue;
    }
    // Keep the policy explicit: rolling filters are based on Asia/Seoul time,
    // then converted back to UTC for the YouTube Data API request.
    const std::time_t now_kst = UtcToKst(now);
    const std::time_t published_after_kst =
        now_kst > mapping.window_seconds ? now_kst - mapping.window_seconds : 0;
    return PublishedWindow{FormatRfc3339Utc(KstToUtc(published_after_kst)),
                           FormatRfc3339Utc(KstToUtc(now_kst))};
  }

  return std::nullopt;
}

std::string BuildSearchUrl(const YouTubeCommand& command,
                           TimeFilter filter,
                           std::time_t now,
                           std::string_view api_key) {
  std::string url = BuildSearchUrlWithoutKey(command, filter, now);
  url += "&key=" + UrlEncode(api_key);
  return url;
}

JsonResult NormalizeSearchResponse(const JsonValue& response,
                                   const YouTubeCommand& command,
                                   TimeFilter filter) {
  const JsonValue items = response.At("items");
  if (!items.IsArray()) {
    return MakeYouTubeError("youtube_response_invalid",
                            "YouTube search response did not contain items.",
                            "Expected an items array in search.list response.");
  }

  JsonValue videos = JsonValue::Array();
  for (std::size_t index = 0; index < items.Size(); ++index) {
    const JsonValue item = items.At(index);
    const JsonValue id = item.At("id");
    const JsonValue snippet = item.At("snippet");
    const std::string video_id = JsonString(id, "videoId");
    if (video_id.empty() || !snippet.IsObject()) {
      continue;
    }

    ArrayAppend(
        videos,
        MakeObject({
            {"videoId", JsonValue::String(video_id)},
            {"title",
             JsonValue::String(CleanText(JsonString(snippet, "title"), 160))},
            {"thumbnail", JsonValue::String(ExtractThumbnailUrl(snippet))},
            {"channelName",
             JsonValue::String(
                 CleanText(JsonString(snippet, "channelTitle"), 80))},
            {"publishedAt", JsonValue::String(JsonString(snippet, "publishedAt"))},
        }));
  }

  JsonValue document = MakeObject({
      {"domain", JsonValue::String("youtube")},
      {"source", JsonValue::String("youtube-data-api-v3")},
      {"query", JsonValue::String(command.query)},
      {"sp", command.sp.empty() ? JsonValue::Null() : JsonValue::String(command.sp)},
      {"timeFilter", JsonValue::String(TimeFilterName(filter))},
      {"count", JsonValue::Integer(command.count)},
      {"videos", std::move(videos)},
  });

  const JsonValue page_info = response.At("pageInfo");
  if (page_info.IsObject() && ObjectHasKey(page_info, "totalResults")) {
    ObjectSet(document, "totalResults",
              JsonValue::Integer(page_info.At("totalResults").AsInt()));
  } else {
    ObjectSet(document, "totalResults", JsonValue::Null());
  }

  return document;
}

JsonResult Execute(const YouTubeCommand& command) {
  const auto parsed_filter = TimeFilterFromLegacySp(command.sp);
  if (std::holds_alternative<AppError>(parsed_filter)) {
    return std::get<AppError>(parsed_filter);
  }

  const TimeFilter filter = std::get<TimeFilter>(parsed_filter);
  const std::time_t now = std::time(nullptr);
  const auto window = BuildPublishedWindow(filter, now);

  if (command.dry_run) {
    JsonValue request = MakeObject({
        {"query", JsonValue::String(command.query)},
        {"sp", command.sp.empty() ? JsonValue::Null() : JsonValue::String(command.sp)},
        {"time_filter", JsonValue::String(TimeFilterName(filter))},
        {"count", JsonValue::Integer(command.count)},
        {"region_code", JsonValue::String("KR")},
        {"relevance_language", JsonValue::String("ko")},
        {"safe_search", JsonValue::String("moderate")},
        {"time_zone", JsonValue::String(kTimeZoneId)},
        {"api_key_env", JsonValue::String(kPrimaryApiKeyEnv)},
        {"api_key_env_fallback", JsonValue::String(kFallbackApiKeyEnv)},
        {"url_preview",
         JsonValue::String(BuildSearchUrlWithoutKey(command, filter, now) +
                           "&key=<redacted>")},
    });
    ObjectSet(request, "published_after",
              window.has_value() ? JsonValue::String(window->published_after)
                                 : JsonValue::Null());
    ObjectSet(request, "published_before",
              window.has_value() ? JsonValue::String(window->published_before)
                                 : JsonValue::Null());

    return MakeObject({
        {"command", JsonValue::String("youtube")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("youtube-data-api-v3")},
        {"request", std::move(request)},
    });
  }

  const auto api_key = ResolveApiKeyConfig();
  if (!api_key.has_value()) {
    return MakeYouTubeError(
        "youtube_api_key_missing",
        "YouTube Data API key is not configured.",
        "Set TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_API_KEY or YOUTUBE_DATA_API_KEY.",
        2);
  }

  const std::string url = BuildSearchUrl(command, filter, now, api_key->api_key);
  const auto response = HttpGetDetailed(url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "youtube_request_failed";
    error.message = "YouTube search request failed.";
    error.hint = BuildSearchUrlWithoutKey(command, filter, now) + " | " + error.hint;
    return error;
  }

  const HttpResponse& http = std::get<HttpResponse>(response);
  const auto parsed = JsonValue::Parse(http.body,
                                       "youtube_response_invalid",
                                       "YouTube search response is not valid JSON.",
                                       6);
  if (std::holds_alternative<AppError>(parsed)) {
    return std::get<AppError>(parsed);
  }

  const JsonValue document = std::get<JsonValue>(parsed);
  if (http.status_code < 200 || http.status_code >= 300) {
    if (const auto api_error = ApiErrorFromResponse(document, http.status_code);
        api_error.has_value()) {
      return *api_error;
    }
    return MakeYouTubeError("youtube_api_error",
                            "YouTube Data API request failed.",
                            "HTTP " + std::to_string(http.status_code));
  }

  if (const auto api_error = ApiErrorFromResponse(document, http.status_code);
      api_error.has_value()) {
    return *api_error;
  }

  return NormalizeSearchResponse(document, command, filter);
}

}  // namespace tizen_tool_domain_fetch::youtube
