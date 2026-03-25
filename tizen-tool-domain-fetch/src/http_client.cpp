#include "tizen_tool_domain_fetch/http_client.hpp"

#include <curl/curl.h>

#include <string>

namespace tizen_tool_domain_fetch {

namespace {

size_t WriteBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* output = static_cast<std::string*>(userdata);
  output->append(ptr, size * nmemb);
  return size * nmemb;
}

std::variant<HttpResponse, AppError> PerformRequest(
    CURL* curl, bool fail_on_http_error) {
  std::string body;
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, fail_on_http_error ? 1L : 0L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "tizen-tool-domain-fetch/0.1.0");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  const CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    return AppError{
        .code = "http_request_failed",
        .message = "Network request failed.",
        .hint = curl_easy_strerror(result),
        .exit_code = 4,
    };
  }

  long status_code = 0;
  const CURLcode info_result =
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  if (info_result != CURLE_OK) {
    return AppError{
        .code = "http_status_failed",
        .message = "Failed to read HTTP response code.",
        .hint = curl_easy_strerror(info_result),
        .exit_code = 4,
    };
  }

  return HttpResponse{
      .status_code = status_code,
      .body = std::move(body),
  };
}

}  // namespace

std::variant<std::string, AppError> HttpGet(std::string_view url,
                                            long connect_timeout_seconds,
                                            long max_timeout_seconds) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return AppError{
        .code = "http_init_failed",
        .message = "Failed to initialize curl.",
        .hint = "Verify libcurl is available at runtime.",
        .exit_code = 3,
    };
  }

  const std::string url_string(url);
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);
  const auto result = PerformRequest(curl, true);
  curl_easy_cleanup(curl);
  if (std::holds_alternative<AppError>(result)) {
    return std::get<AppError>(result);
  }
  return std::get<HttpResponse>(result).body;
}

std::variant<HttpResponse, AppError> HttpGetDetailed(
    std::string_view url,
    long connect_timeout_seconds,
    long max_timeout_seconds) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return AppError{
        .code = "http_init_failed",
        .message = "Failed to initialize curl.",
        .hint = "Verify libcurl is available at runtime.",
        .exit_code = 3,
    };
  }

  const std::string url_string(url);
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);
  const auto result = PerformRequest(curl, false);
  curl_easy_cleanup(curl);
  return result;
}

std::variant<std::string, AppError> HttpPostForm(
    std::string_view url,
    std::string_view form_body,
    std::string_view accept,
    long connect_timeout_seconds,
    long max_timeout_seconds) {
  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    return AppError{
        .code = "http_init_failed",
        .message = "Failed to initialize curl.",
        .hint = "Verify libcurl is available at runtime.",
        .exit_code = 3,
    };
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers,
                              ("Accept: " + std::string(accept)).c_str());
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");

  const std::string url_string(url);
  const std::string body_string(form_body);
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_string.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);

  const auto result = PerformRequest(curl, true);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  if (std::holds_alternative<AppError>(result)) {
    return std::get<AppError>(result);
  }
  return std::get<HttpResponse>(result).body;
}

}  // namespace tizen_tool_domain_fetch
