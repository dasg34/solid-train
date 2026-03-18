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

  Source source = Source::kOpenMeteo;
  OutputFormat format = OutputFormat::kJson;
  std::string city = "서울";
  std::string district = "중구";
  std::optional<double> latitude;
  std::optional<double> longitude;
  int hours = 6;
  bool dry_run = false;
};

struct NewsCommand {
  enum class Source { kMock, kYonhapRss, kGoogleNewsRss };

  Source source = Source::kYonhapRss;
  OutputFormat format = OutputFormat::kJson;
  std::string rss_url = "https://www.yonhapnewstv.co.kr/browse/feed/";
  std::string query;
  int count = 6;
  bool dry_run = false;
};

struct FinanceCommand {
  enum class Source { kMock, kNaverPublic };

  Source source = Source::kNaverPublic;
  OutputFormat format = OutputFormat::kJson;
  std::string watchlist = "005930:삼성전자,000660:SK하이닉스,035420:NAVER";
  bool dry_run = false;
};

struct CommuteCommand {
  enum class Source { kMock, kOsrm };
  enum class Profile { kDriving, kWalking };

  Source source = Source::kOsrm;
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

  Source source = Source::kTheSportsDb;
  OutputFormat format = OutputFormat::kJson;
  std::string league = "kleague1";
  std::string league_id;
  std::string league_name;
  bool dry_run = false;
};

struct ScheduleCommand {
  enum class Source { kMock, kIcsUrl, kIcsFile };

  Source source = Source::kIcsUrl;
  OutputFormat format = OutputFormat::kJson;
  std::string ics_url = "https://holidays.hyunbin.page/basic.ics";
  std::string ics_file;
  int days = 2;
  int max_events = 6;
  std::string now;
  bool dry_run = false;
};

struct TravelCommand {
  enum class Source { kMock, kAirportKr };

  Source source = Source::kAirportKr;
  OutputFormat format = OutputFormat::kJson;
  std::string now;
  std::string date;
  int window_hours = 4;
  std::string from_time;
  std::string to_time;
  std::string flight_number;
  std::string destination_code;
  std::string terminal;
  std::string airline;
  bool include_codeshare = false;
  bool dry_run = false;
};

struct EmergencyCommand {
  enum class Source {
    kMock,
    kKmaSpecialReport,
    kKmaEarthquake,
    kKmaCombined,
  };

  Source source = Source::kKmaCombined;
  OutputFormat format = OutputFormat::kJson;
  double min_magnitude = 3.0;
  int max_age_days = 7;
  std::string now;
  bool dry_run = false;
};

struct DailyCommand {
  enum class Source { kMock, kComposeLive };
  enum class WeatherSource { kOpenMeteo, kMock, kSkip };
  enum class NewsSource { kYonhapRss, kMock, kSkip };
  enum class ScheduleSource { kIcsUrl, kIcsFile, kMock, kSkip };
  enum class CommuteSource { kOsrm, kMock, kSkip };

  Source source = Source::kComposeLive;
  OutputFormat format = OutputFormat::kJson;
  WeatherSource weather_source = WeatherSource::kOpenMeteo;
  NewsSource news_source = NewsSource::kYonhapRss;
  ScheduleSource schedule_source = ScheduleSource::kIcsUrl;
  CommuteSource commute_source = CommuteSource::kOsrm;
  double latitude = 37.5665;
  double longitude = 126.9780;
  std::string city = "서울";
  std::string district = "중구";
  int weather_hours = 4;
  std::string rss_url = "https://www.yonhapnewstv.co.kr/browse/feed/";
  int news_count = 5;
  std::string ics_url = "https://holidays.hyunbin.page/basic.ics";
  std::string ics_file;
  int schedule_days = 2;
  int schedule_max_events = 6;
  std::string schedule_now;
  std::string commute_origin = "서울시청";
  std::string commute_destination = "강남역";
  std::string commute_origin_label;
  std::string commute_destination_label;
  CommuteCommand::Profile commute_profile = CommuteCommand::Profile::kDriving;
  std::string commute_arrive_by;
  std::string commute_now;
  int commute_buffer_minutes = 8;
  bool dry_run = false;
};

struct ScenarioCommand {
  enum class Kind {
    kFamily,
    kMealDelivery,
    kMedia,
    kShopping,
    kSmartHome,
    kWellness,
  };

  Kind kind = Kind::kFamily;
  OutputFormat format = OutputFormat::kJson;
  std::string source;
  bool dry_run = false;
};

using Command = std::variant<DescribeCommand,
                             WeatherCommand,
                             NewsCommand,
                             FinanceCommand,
                             CommuteCommand,
                             SportsCommand,
                             ScheduleCommand,
                             TravelCommand,
                             EmergencyCommand,
                             DailyCommand,
                             ScenarioCommand>;

std::variant<Command, AppError> ParseCommand(int argc, char** argv);
std::string RenderHelp();
std::string_view ToString(OutputFormat format);
JsonValue BuildDescribeDocument(const std::optional<std::string>& target);

}  // namespace tv_fetch
