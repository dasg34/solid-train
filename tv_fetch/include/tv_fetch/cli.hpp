#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "tv_fetch/error.hpp"
#include "tv_fetch/json.hpp"

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
  std::optional<double> latitude;
  std::optional<double> longitude;
  int hours = 6;
  bool dry_run = false;
};

struct NewsCommand {
  enum class Source { kMock, kYonhapRss };

  Source source = Source::kMock;
  OutputFormat format = OutputFormat::kJson;
  std::string rss_url = "https://www.yonhapnewstv.co.kr/browse/feed/";
  int count = 6;
  bool dry_run = false;
};

struct FinanceCommand {
  enum class Source { kMock, kNaverPublic };

  Source source = Source::kMock;
  OutputFormat format = OutputFormat::kJson;
  std::string watchlist = "005930:삼성전자,000660:SK하이닉스,035420:NAVER";
  bool dry_run = false;
};

struct CommuteCommand {
  enum class Source { kMock, kOsrm };
  enum class Profile { kDriving, kWalking };

  Source source = Source::kMock;
  OutputFormat format = OutputFormat::kJson;
  std::string origin = "서울시청";
  std::string destination = "강남역";
  std::string origin_label;
  std::string destination_label;
  Profile profile = Profile::kDriving;
  std::string arrive_by;
  std::string now;
  int buffer_minutes = 8;
  bool dry_run = false;
};

struct SportsCommand {
  enum class Source { kMock, kTheSportsDb };

  Source source = Source::kMock;
  OutputFormat format = OutputFormat::kJson;
  std::string league = "kleague1";
  std::string league_id;
  std::string league_name;
  bool dry_run = false;
};

using Command = std::variant<DescribeCommand,
                             WeatherCommand,
                             NewsCommand,
                             FinanceCommand,
                             CommuteCommand,
                             SportsCommand>;

std::variant<Command, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
std::string_view ToString(OutputFormat format);
JsonValue BuildDescribeDocument(const std::optional<std::string>& target);

}  // namespace tv_fetch
