#include "tv_fetch/http_client.hpp"

#include <curl/curl.h>

#include <string>

namespace tv_fetch {

namespace {

size_t WriteBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* output = static_cast<std::string*>(userdata);
  output->append(ptr, size * nmemb);
  return size * nmemb;
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
  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, max_timeout_seconds);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "tv_fetch/0.1.0");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

  const CURLcode result = curl_easy_perform(curl);
  if (result != CURLE_OK) {
    const std::string error_message = curl_easy_strerror(result);
    curl_easy_cleanup(curl);
    return AppError{
        .code = "http_request_failed",
        .message = "Network request failed.",
        .hint = error_message,
        .exit_code = 4,
    };
  }

  curl_easy_cleanup(curl);
  return body;
}

}  // namespace tv_fetch
