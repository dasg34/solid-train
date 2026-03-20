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

std::variant<std::string, AppError> PerformRequest(
    CURL* curl, std::string* body) {
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "tizen-tool-domain-fetch/0.1.0");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);

  const CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    return AppError{
        .code = "http_request_failed",
        .message = "Network request failed.",
        .hint = curl_easy_strerror(result),
        .exit_code = 4,
    };
  }
  return *body;
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

  std::string body;
  const std::string url_string(url);
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);
  const auto result = PerformRequest(curl, &body);
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

  std::string body;
  const std::string url_string(url);
  const std::string body_string(form_body);
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_string.c_str());
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);

  const auto result = PerformRequest(curl, &body);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return result;
}

}  // namespace tizen_tool_domain_fetch
