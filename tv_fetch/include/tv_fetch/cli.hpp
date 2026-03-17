#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <nlohmann/json.hpp>

#include "tv_fetch/error.hpp"

namespace tv_fetch {

enum class OutputFormat { kJson, kPretty };

struct DescribeCommand {
  std::optional<std::string> target;
  OutputFormat format = OutputFormat::kJson;
};

struct WeatherCommand {
  enum class Source { kMock, kOpenMeteo };

  Source source = Source::kMock;
  OutputFormat format = OutputFormat::kJson;
  std::string city = "서울";
  std::string district = "중구";
  double latitude = 37.5665;
  double longitude = 126.9780;
  int hours = 6;
  bool dry_run = false;
};

using Command = std::variant<DescribeCommand, WeatherCommand>;

std::variant<Command, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
std::string_view ToString(OutputFormat format);
nlohmann::json BuildDescribeDocument(const std::optional<std::string>& target);

}  // namespace tv_fetch
