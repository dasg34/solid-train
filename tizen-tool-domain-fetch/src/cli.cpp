#include "tizen_tool_domain_fetch/cli.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

#include "tizen_tool_domain_fetch/scenario/scenario_fetcher.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch {

namespace {

constexpr std::array<std::string_view, 16> kDescribeTargets = {
    "weather",
    "news",
    "youtube",
    "finance",
    "commute",
    "sports",
    "daily",
    "emergency",
    "family",
    "meal-delivery",
    "media",
    "schedule",
    "shopping",
    "smart-home",
    "travel",
    "wellness",
};

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

bool ContainsControlChars(std::string_view value) {
  return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
    return std::iscntrl(ch) != 0;
  });
}

AppError InvalidArguments(std::string message, std::string hint = {}) {
  return AppError{
      .code = "invalid_arguments",
      .message = std::move(message),
      .hint = std::move(hint),
      .exit_code = 2,
  };
}

bool ParseDouble(std::string_view value, double* out) {
  try {
    std::size_t index = 0;
    const auto parsed = std::stod(std::string(value), &index);
    if (index != value.size()) {
      return false;
    }
    *out = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

bool ParseInt(std::string_view value, int* out) {
  try {
    std::size_t index = 0;
    const auto parsed = std::stoi(std::string(value), &index);
    if (index != value.size()) {
      return false;
    }
    *out = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

std::variant<OutputFormat, AppError> ParseOutputFormat(std::string_view value) {
  if (value == "json") {
    return OutputFormat::kJson;
  }
  if (value == "pretty") {
    return OutputFormat::kPretty;
  }
  return InvalidArguments(
      "Unsupported format value.",
      "Use --format json or --format pretty.");
}

std::variant<DescribeCommand, AppError> ParseDescribe(
    const std::vector<std::string_view>& args) {
  DescribeCommand command;
  std::size_t index = 0;
  if (index < args.size() && !StartsWith(args[index], "--")) {
    command.target = std::string(args[index]);
    ++index;
  }

  while (index < args.size()) {
    const auto arg = args[index];
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    return InvalidArguments(
        "Unsupported describe option.",
        "Use tizen-tool-domain-fetch describe [weather|news|youtube|finance|commute|sports|daily|emergency|family|meal-delivery|media|schedule|shopping|smart-home|travel|wellness] [--format json|pretty].");
  }

  return command;
}

std::variant<WeatherCommand, AppError> ParseWeather(
    const std::vector<std::string_view>& args) {
  WeatherCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = WeatherCommand::Source::kMock;
      } else if (value == "open-meteo") {
        command.source = WeatherCommand::Source::kOpenMeteo;
      } else {
        return InvalidArguments(
            "Unsupported weather source.",
            "Use --source mock or --source open-meteo.");
      }
      index += 2;
      continue;
    }
    if (arg == "--city") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --city.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("City contains control characters.");
      }
      command.city = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--district") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --district.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("District contains control characters.");
      }
      command.district = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--latitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --latitude.");
      }
      double latitude = 0.0;
      if (!ParseDouble(args[index + 1], &latitude) || latitude < -90.0 ||
          latitude > 90.0) {
        return InvalidArguments(
            "Latitude must be a number between -90 and 90.");
      }
      command.latitude = latitude;
      index += 2;
      continue;
    }
    if (arg == "--longitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --longitude.");
      }
      double longitude = 0.0;
      if (!ParseDouble(args[index + 1], &longitude) || longitude < -180.0 ||
          longitude > 180.0) {
        return InvalidArguments(
            "Longitude must be a number between -180 and 180.");
      }
      command.longitude = longitude;
      index += 2;
      continue;
    }
    if (arg == "--hours") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --hours.");
      }
      int hours = 0;
      if (!ParseInt(args[index + 1], &hours) || hours < 1 || hours > 24) {
        return InvalidArguments("Hours must be an integer between 1 and 24.");
      }
      command.hours = hours;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported weather option.",
        "Use tizen-tool-domain-fetch weather [--source mock|open-meteo] [--city ...] "
        "[--district ...] [--latitude ...] [--longitude ...] [--hours ...] "
        "[--dry-run] [--format json|pretty].");
  }

  if (command.latitude.has_value() != command.longitude.has_value()) {
    return InvalidArguments(
        "Latitude and longitude must be provided together.",
        "Either provide both --latitude and --longitude, or omit both to use geocoding.");
  }

  if (!command.latitude.has_value() && command.city.empty() &&
      command.district.empty()) {
    return InvalidArguments(
        "Weather requires a location input.",
        "Use --city/--district or provide both --latitude and --longitude.");
  }

  return command;
}

std::variant<NewsCommand, AppError> ParseNews(
    const std::vector<std::string_view>& args) {
  NewsCommand command;
  bool source_explicit = false;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = NewsCommand::Source::kMock;
      } else if (value == "yonhap-rss") {
        command.source = NewsCommand::Source::kYonhapRss;
      } else if (value == "google-news-rss") {
        command.source = NewsCommand::Source::kGoogleNewsRss;
      } else {
        return InvalidArguments(
            "Unsupported news source.",
            "Use --source mock, --source yonhap-rss, or --source google-news-rss.");
      }
      source_explicit = true;
      index += 2;
      continue;
    }
    if (arg == "--rss-url") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --rss-url.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("RSS URL contains control characters.");
      }
      command.rss_url = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--query") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --query.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Query contains control characters.");
      }
      command.query = std::string(args[index + 1]);
      if (!source_explicit) {
        command.source = NewsCommand::Source::kGoogleNewsRss;
      } else if (source_explicit &&
                 command.source != NewsCommand::Source::kGoogleNewsRss) {
        return InvalidArguments(
            "News search requires the google-news-rss source.",
            "Use tizen-tool-domain-fetch news --query ... or explicitly set --source google-news-rss.");
      }
      index += 2;
      continue;
    }
    if (arg == "--count") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --count.");
      }
      int count = 0;
      if (!ParseInt(args[index + 1], &count) || count < 1 || count > 12) {
        return InvalidArguments("Count must be an integer between 1 and 12.");
      }
      command.count = count;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported news option.",
        "Use tizen-tool-domain-fetch news [--source mock|yonhap-rss|google-news-rss] "
        "[--rss-url ...] [--query ...] [--count 1-12] [--dry-run] "
        "[--format json|pretty].");
  }

  if (command.source == NewsCommand::Source::kGoogleNewsRss &&
      command.query.empty()) {
    return InvalidArguments(
        "Google News search requires a query.",
        "Use tizen-tool-domain-fetch news --query 반도체 [--count ...] [--format pretty].");
  }

  return command;
}

std::variant<YouTubeCommand, AppError> ParseYouTube(
    const std::vector<std::string_view>& args) {
  YouTubeCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--query") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --query.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Query contains control characters.");
      }
      command.query = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--sp") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --sp.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("sp contains control characters.");
      }
      command.sp = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--count") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --count.");
      }
      int count = 0;
      if (!ParseInt(args[index + 1], &count) || count < 1 || count > 50) {
        return InvalidArguments("Count must be an integer between 1 and 50.");
      }
      command.count = count;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported youtube option.",
        "Use tizen-tool-domain-fetch youtube --query ... [--sp ...] [--count 1-50] "
        "[--dry-run] [--format json|pretty].");
  }

  if (command.query.empty()) {
    return InvalidArguments(
        "YouTube search requires a query.",
        "Use tizen-tool-domain-fetch youtube --query 아이유 [--sp ...] [--count ...].");
  }

  return command;
}

std::variant<FinanceCommand, AppError> ParseFinance(
    const std::vector<std::string_view>& args) {
  FinanceCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = FinanceCommand::Source::kMock;
      } else if (value == "naver-public") {
        command.source = FinanceCommand::Source::kNaverPublic;
      } else {
        return InvalidArguments(
            "Unsupported finance source.",
            "Use --source mock or --source naver-public.");
      }
      index += 2;
      continue;
    }
    if (arg == "--watchlist") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --watchlist.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Watchlist contains control characters.");
      }
      command.watchlist = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported finance option.",
        "Use tizen-tool-domain-fetch finance [--source mock|naver-public] [--watchlist ...] "
        "[--dry-run] [--format json|pretty].");
  }

  return command;
}

std::variant<CommuteCommand, AppError> ParseCommute(
    const std::vector<std::string_view>& args) {
  CommuteCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = CommuteCommand::Source::kMock;
      } else if (value == "osrm") {
        command.source = CommuteCommand::Source::kOsrm;
      } else {
        return InvalidArguments(
            "Unsupported commute source.",
            "Use --source mock or --source osrm.");
      }
      index += 2;
      continue;
    }
    if (arg == "--origin") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --origin.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Origin contains control characters.");
      }
      command.origin = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--destination") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --destination.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Destination contains control characters.");
      }
      command.destination = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--origin-label") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --origin-label.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Origin label contains control characters.");
      }
      command.origin_label = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--destination-label") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --destination-label.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Destination label contains control characters.");
      }
      command.destination_label = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--profile") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --profile.");
      }
      const auto value = args[index + 1];
      if (value == "driving") {
        command.profile = CommuteCommand::Profile::kDriving;
      } else if (value == "walking") {
        command.profile = CommuteCommand::Profile::kWalking;
      } else {
        return InvalidArguments(
            "Unsupported commute profile.",
            "Use --profile driving or --profile walking.");
      }
      index += 2;
      continue;
    }
    if (arg == "--arrive-by") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --arrive-by.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Arrival time contains control characters.");
      }
      command.arrive_by = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --now.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Now override contains control characters.");
      }
      command.now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--buffer-minutes") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --buffer-minutes.");
      }
      int minutes = 0;
      if (!ParseInt(args[index + 1], &minutes) || minutes < 0 ||
          minutes > 180) {
        return InvalidArguments(
            "Buffer minutes must be an integer between 0 and 180.");
      }
      command.buffer_minutes = minutes;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported commute option.",
        "Use tizen-tool-domain-fetch commute [--source mock|osrm] [--origin ...] "
        "[--destination ...] [--origin-label ...] [--destination-label ...] "
        "[--profile driving|walking] [--arrive-by ...] [--now ...] "
        "[--buffer-minutes 0-180] [--dry-run] [--format json|pretty].");
  }

  if (command.origin.empty() || command.destination.empty()) {
    return InvalidArguments(
        "Commute requires both origin and destination.",
        "Use --origin '망포역' --destination '서초구청'.");
  }

  return command;
}

std::variant<SportsCommand, AppError> ParseSports(
    const std::vector<std::string_view>& args) {
  SportsCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = SportsCommand::Source::kMock;
      } else if (value == "thesportsdb") {
        command.source = SportsCommand::Source::kTheSportsDb;
      } else {
        return InvalidArguments(
            "Unsupported sports source.",
            "Use --source mock or --source thesportsdb.");
      }
      index += 2;
      continue;
    }
    if (arg == "--league") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --league.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("League contains control characters.");
      }
      command.league = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--league-id") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --league-id.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("League ID contains control characters.");
      }
      command.league_id = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--league-name") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --league-name.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("League name contains control characters.");
      }
      command.league_name = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported sports option.",
        "Use tizen-tool-domain-fetch sports [--source mock|thesportsdb] [--league ...] "
        "[--league-id ...] [--league-name ...] [--dry-run] "
        "[--format json|pretty].");
  }

  return command;
}

std::variant<ScheduleCommand, AppError> ParseSchedule(
    const std::vector<std::string_view>& args) {
  ScheduleCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = ScheduleCommand::Source::kMock;
      } else if (value == "ics-url") {
        command.source = ScheduleCommand::Source::kIcsUrl;
      } else if (value == "ics-file") {
        command.source = ScheduleCommand::Source::kIcsFile;
      } else {
        return InvalidArguments(
            "Unsupported schedule source.",
            "Use --source mock, --source ics-url, or --source ics-file.");
      }
      index += 2;
      continue;
    }
    if (arg == "--ics-url") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --ics-url.");
      }
      command.ics_url = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--ics-file") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --ics-file.");
      }
      command.ics_file = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--days") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --days.");
      }
      int days = 0;
      if (!ParseInt(args[index + 1], &days) || days < 1 || days > 30) {
        return InvalidArguments("Days must be an integer between 1 and 30.");
      }
      command.days = days;
      index += 2;
      continue;
    }
    if (arg == "--max-events") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --max-events.");
      }
      int max_events = 0;
      if (!ParseInt(args[index + 1], &max_events) || max_events < 1 ||
          max_events > 20) {
        return InvalidArguments(
            "Max events must be an integer between 1 and 20.");
      }
      command.max_events = max_events;
      index += 2;
      continue;
    }
    if (arg == "--now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --now.");
      }
      command.now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported schedule option.",
        "Use tizen-tool-domain-fetch schedule [--source mock|ics-url|ics-file] [--ics-url ...] [--ics-file ...] [--days 1-30] [--max-events 1-20] [--now ...] [--dry-run] [--format json|pretty].");
  }

  return command;
}

std::variant<TravelCommand, AppError> ParseTravel(
    const std::vector<std::string_view>& args) {
  TravelCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = TravelCommand::Source::kMock;
      } else if (value == "airport-kr") {
        command.source = TravelCommand::Source::kAirportKr;
      } else {
        return InvalidArguments(
            "Unsupported travel source.",
            "Use --source mock or --source airport-kr.");
      }
      index += 2;
      continue;
    }
    if (arg == "--now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --now.");
      }
      command.now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--date") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --date.");
      }
      command.date = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--window-hours") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --window-hours.");
      }
      int hours = 0;
      if (!ParseInt(args[index + 1], &hours) || hours < 1 || hours > 24) {
        return InvalidArguments(
            "Window hours must be an integer between 1 and 24.");
      }
      command.window_hours = hours;
      index += 2;
      continue;
    }
    if (arg == "--from-time") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --from-time.");
      }
      command.from_time = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--to-time") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --to-time.");
      }
      command.to_time = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--flight-number") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --flight-number.");
      }
      command.flight_number = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--destination-code") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --destination-code.");
      }
      command.destination_code = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--terminal") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --terminal.");
      }
      const auto value = args[index + 1];
      if (value != "T1" && value != "T2") {
        return InvalidArguments("Terminal must be T1 or T2.");
      }
      command.terminal = std::string(value);
      index += 2;
      continue;
    }
    if (arg == "--airline") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --airline.");
      }
      command.airline = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--include-codeshare") {
      command.include_codeshare = true;
      ++index;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported travel option.",
        "Use tizen-tool-domain-fetch travel [--source mock|airport-kr] [--date ...] [--window-hours 1-24] [--from-time ...] [--to-time ...] [--flight-number ...] [--destination-code ...] [--terminal T1|T2] [--airline ...] [--include-codeshare] [--now ...] [--dry-run] [--format json|pretty].");
  }

  return command;
}

std::variant<EmergencyCommand, AppError> ParseEmergency(
    const std::vector<std::string_view>& args) {
  EmergencyCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = EmergencyCommand::Source::kMock;
      } else if (value == "kma-special-report") {
        command.source = EmergencyCommand::Source::kKmaSpecialReport;
      } else if (value == "kma-earthquake") {
        command.source = EmergencyCommand::Source::kKmaEarthquake;
      } else if (value == "kma-combined") {
        command.source = EmergencyCommand::Source::kKmaCombined;
      } else {
        return InvalidArguments(
            "Unsupported emergency source.",
            "Use --source mock, --source kma-special-report, --source kma-earthquake, or --source kma-combined.");
      }
      index += 2;
      continue;
    }
    if (arg == "--min-magnitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --min-magnitude.");
      }
      double magnitude = 0.0;
      if (!ParseDouble(args[index + 1], &magnitude) || magnitude < 0.0 ||
          magnitude > 10.0) {
        return InvalidArguments(
            "Min magnitude must be a number between 0 and 10.");
      }
      command.min_magnitude = magnitude;
      index += 2;
      continue;
    }
    if (arg == "--max-age-days") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --max-age-days.");
      }
      int days = 0;
      if (!ParseInt(args[index + 1], &days) || days < 1 || days > 30) {
        return InvalidArguments(
            "Max age days must be an integer between 1 and 30.");
      }
      command.max_age_days = days;
      index += 2;
      continue;
    }
    if (arg == "--now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --now.");
      }
      command.now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported emergency option.",
        "Use tizen-tool-domain-fetch emergency [--source mock|kma-special-report|kma-earthquake|kma-combined] [--min-magnitude ...] [--max-age-days 1-30] [--now ...] [--dry-run] [--format json|pretty].");
  }

  return command;
}

std::variant<DailyCommand, AppError> ParseDaily(
    const std::vector<std::string_view>& args) {
  DailyCommand command;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      const auto value = args[index + 1];
      if (value == "mock") {
        command.source = DailyCommand::Source::kMock;
      } else if (value == "compose-live") {
        command.source = DailyCommand::Source::kComposeLive;
      } else {
        return InvalidArguments(
            "Unsupported daily source.",
            "Use --source mock or --source compose-live.");
      }
      index += 2;
      continue;
    }
    if (arg == "--weather-source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --weather-source.");
      }
      const auto value = args[index + 1];
      if (value == "open-meteo") {
        command.weather_source = DailyCommand::WeatherSource::kOpenMeteo;
      } else if (value == "mock") {
        command.weather_source = DailyCommand::WeatherSource::kMock;
      } else if (value == "skip") {
        command.weather_source = DailyCommand::WeatherSource::kSkip;
      } else {
        return InvalidArguments(
            "Unsupported weather source for daily.",
            "Use --weather-source open-meteo, mock, or skip.");
      }
      index += 2;
      continue;
    }
    if (arg == "--news-source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --news-source.");
      }
      const auto value = args[index + 1];
      if (value == "yonhap-rss") {
        command.news_source = DailyCommand::NewsSource::kYonhapRss;
      } else if (value == "mock") {
        command.news_source = DailyCommand::NewsSource::kMock;
      } else if (value == "skip") {
        command.news_source = DailyCommand::NewsSource::kSkip;
      } else {
        return InvalidArguments(
            "Unsupported news source for daily.",
            "Use --news-source yonhap-rss, mock, or skip.");
      }
      index += 2;
      continue;
    }
    if (arg == "--schedule-source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --schedule-source.");
      }
      const auto value = args[index + 1];
      if (value == "ics-url") {
        command.schedule_source = DailyCommand::ScheduleSource::kIcsUrl;
      } else if (value == "ics-file") {
        command.schedule_source = DailyCommand::ScheduleSource::kIcsFile;
      } else if (value == "mock") {
        command.schedule_source = DailyCommand::ScheduleSource::kMock;
      } else if (value == "skip") {
        command.schedule_source = DailyCommand::ScheduleSource::kSkip;
      } else {
        return InvalidArguments(
            "Unsupported schedule source for daily.",
            "Use --schedule-source ics-url, ics-file, mock, or skip.");
      }
      index += 2;
      continue;
    }
    if (arg == "--commute-source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-source.");
      }
      const auto value = args[index + 1];
      if (value == "osrm") {
        command.commute_source = DailyCommand::CommuteSource::kOsrm;
      } else if (value == "mock") {
        command.commute_source = DailyCommand::CommuteSource::kMock;
      } else if (value == "skip") {
        command.commute_source = DailyCommand::CommuteSource::kSkip;
      } else {
        return InvalidArguments(
            "Unsupported commute source for daily.",
            "Use --commute-source osrm, mock, or skip.");
      }
      index += 2;
      continue;
    }
    if (arg == "--latitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --latitude.");
      }
      double latitude = 0.0;
      if (!ParseDouble(args[index + 1], &latitude) || latitude < -90.0 ||
          latitude > 90.0) {
        return InvalidArguments(
            "Latitude must be a number between -90 and 90.");
      }
      command.latitude = latitude;
      index += 2;
      continue;
    }
    if (arg == "--longitude") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --longitude.");
      }
      double longitude = 0.0;
      if (!ParseDouble(args[index + 1], &longitude) || longitude < -180.0 ||
          longitude > 180.0) {
        return InvalidArguments(
            "Longitude must be a number between -180 and 180.");
      }
      command.longitude = longitude;
      index += 2;
      continue;
    }
    if (arg == "--city") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --city.");
      }
      command.city = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--district") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --district.");
      }
      command.district = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--weather-hours") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --weather-hours.");
      }
      int hours = 0;
      if (!ParseInt(args[index + 1], &hours) || hours < 1 || hours > 12) {
        return InvalidArguments(
            "Weather hours must be an integer between 1 and 12.");
      }
      command.weather_hours = hours;
      index += 2;
      continue;
    }
    if (arg == "--rss-url") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --rss-url.");
      }
      command.rss_url = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--news-count") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --news-count.");
      }
      int count = 0;
      if (!ParseInt(args[index + 1], &count) || count < 1 || count > 12) {
        return InvalidArguments(
            "News count must be an integer between 1 and 12.");
      }
      command.news_count = count;
      index += 2;
      continue;
    }
    if (arg == "--ics-url") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --ics-url.");
      }
      command.ics_url = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--ics-file") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --ics-file.");
      }
      command.ics_file = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--schedule-days") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --schedule-days.");
      }
      int days = 0;
      if (!ParseInt(args[index + 1], &days) || days < 1 || days > 30) {
        return InvalidArguments(
            "Schedule days must be an integer between 1 and 30.");
      }
      command.schedule_days = days;
      index += 2;
      continue;
    }
    if (arg == "--schedule-max-events") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --schedule-max-events.");
      }
      int max_events = 0;
      if (!ParseInt(args[index + 1], &max_events) || max_events < 1 ||
          max_events > 20) {
        return InvalidArguments(
            "Schedule max events must be an integer between 1 and 20.");
      }
      command.schedule_max_events = max_events;
      index += 2;
      continue;
    }
    if (arg == "--schedule-now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --schedule-now.");
      }
      command.schedule_now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-origin") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-origin.");
      }
      command.commute_origin = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-destination") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-destination.");
      }
      command.commute_destination = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-origin-label") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-origin-label.");
      }
      command.commute_origin_label = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-destination-label") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-destination-label.");
      }
      command.commute_destination_label = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-profile") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-profile.");
      }
      const auto value = args[index + 1];
      if (value == "driving") {
        command.commute_profile = CommuteCommand::Profile::kDriving;
      } else if (value == "walking") {
        command.commute_profile = CommuteCommand::Profile::kWalking;
      } else {
        return InvalidArguments(
            "Unsupported commute profile for daily.",
            "Use --commute-profile driving or --commute-profile walking.");
      }
      index += 2;
      continue;
    }
    if (arg == "--commute-arrive-by") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-arrive-by.");
      }
      command.commute_arrive_by = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-now") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-now.");
      }
      command.commute_now = std::string(args[index + 1]);
      index += 2;
      continue;
    }
    if (arg == "--commute-buffer-minutes") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --commute-buffer-minutes.");
      }
      int minutes = 0;
      if (!ParseInt(args[index + 1], &minutes) || minutes < 0 ||
          minutes > 180) {
        return InvalidArguments(
            "Commute buffer minutes must be an integer between 0 and 180.");
      }
      command.commute_buffer_minutes = minutes;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported daily option.",
        "Use tizen-tool-domain-fetch daily [--source mock|compose-live] [--weather-source ...] [--news-source ...] [--schedule-source ...] [--commute-source ...] [--city ...] [--district ...] [--rss-url ...] [--ics-url ...] [--commute-origin ...] [--dry-run] [--format json|pretty].");
  }

  return command;
}

std::variant<ScenarioCommand, AppError> ParseScenario(
    ScenarioCommand::Kind kind, const std::vector<std::string_view>& args) {
  ScenarioCommand command;
  command.kind = kind;
  bool source_explicit = false;

  for (std::size_t index = 0; index < args.size();) {
    const auto arg = args[index];
    if (arg == "--source") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --source.");
      }
      if (ContainsControlChars(args[index + 1])) {
        return InvalidArguments("Source contains control characters.");
      }
      command.source = std::string(args[index + 1]);
      source_explicit = true;
      index += 2;
      continue;
    }
    if (arg == "--format") {
      if (index + 1 >= args.size()) {
        return InvalidArguments("Missing value for --format.");
      }
      const auto parsed = ParseOutputFormat(args[index + 1]);
      if (std::holds_alternative<AppError>(parsed)) {
        return std::get<AppError>(parsed);
      }
      command.format = std::get<OutputFormat>(parsed);
      index += 2;
      continue;
    }
    if (arg == "--dry-run") {
      command.dry_run = true;
      ++index;
      continue;
    }
    return InvalidArguments(
        "Unsupported scenario option.",
        "Use tizen-tool-domain-fetch " + std::string(scenario::CommandName(kind)) +
            " --source mock [--dry-run] [--format json|pretty].");
  }

  if (!source_explicit) {
    return InvalidArguments(
        "This scenario does not have a live source yet.",
        "Use tizen-tool-domain-fetch " + std::string(scenario::CommandName(kind)) +
            " --source mock [--format json|pretty].");
  }

  if (command.source != "mock") {
    return InvalidArguments(
        "This scenario currently supports mock source only.",
        "Use tizen-tool-domain-fetch " + std::string(scenario::CommandName(kind)) +
            " --source mock [--format json|pretty].");
  }

  return command;
}

JsonValue DomainDescribeBase(std::string_view name,
                             std::string_view description,
                             bool supports_live,
                             std::string_view default_source,
                             JsonValue sources,
                             JsonValue parameters,
                             JsonValue output_shape) {
  return MakeObject({
      {"name", JsonValue::String(name)},
      {"description", JsonValue::String(description)},
      {"supports_live", JsonValue::Boolean(supports_live)},
      {"default_source", JsonValue::String(default_source)},
      {"sources", std::move(sources)},
      {"parameters", std::move(parameters)},
      {"output_shape", std::move(output_shape)},
  });
}

JsonValue WeatherDescribeDocument() {
  return DomainDescribeBase(
      "weather",
      "Fetch and normalize Korea-first weather context for TV composition.",
      true,
      "open-meteo",
      MakeArray({JsonValue::String("mock"), JsonValue::String("open-meteo")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("mock"), JsonValue::String("open-meteo")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--city")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--district")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--latitude")},
              {"type", JsonValue::String("number")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--longitude")},
              {"type", JsonValue::String("number")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--hours")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(24)})},
              {"default", JsonValue::Integer(6)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("weather")},
          {"source", JsonValue::String("mock|open-meteo")},
          {"location", JsonValue::String("object")},
          {"updated_at", JsonValue::String("ISO-8601 string")},
          {"headline", JsonValue::String("string")},
          {"current", JsonValue::String("object")},
          {"alert", JsonValue::String("object")},
          {"hourly", JsonValue::String("array")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue NewsDescribeDocument() {
  JsonValue document = DomainDescribeBase(
      "news",
      "Fetch headline-first news context for TV composition. Without --query it returns latest headlines; with --query it performs search.",
      true,
      "yonhap-rss",
      MakeArray({JsonValue::String("mock"),
                 JsonValue::String("yonhap-rss"),
                 JsonValue::String("google-news-rss")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
               {"values",
                MakeArray(
                   {JsonValue::String("mock"),
                    JsonValue::String("yonhap-rss"),
                    JsonValue::String("google-news-rss")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--rss-url")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--query")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"description",
               JsonValue::String(
                   "When provided, tizen-tool-domain-fetch news automatically switches to google-news-rss unless --source is explicitly set.")},
          }),
          MakeObject({
              {"name", JsonValue::String("--count")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(12)})},
              {"default", JsonValue::Integer(6)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("news")},
          {"source", JsonValue::String("mock|yonhap-rss|google-news-rss")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
  ObjectSet(
      document, "mode_selection",
      MakeObject({
          {"default_mode", JsonValue::String("latest-headlines")},
          {"default_source", JsonValue::String("yonhap-rss")},
          {"when_query_is_missing",
           JsonValue::String(
               "Return the latest headline feed from Yonhap RSS.")},
          {"when_query_is_present",
           JsonValue::String(
               "Treat the request as search and use google-news-rss.")},
          {"query_requires_search_source", JsonValue::Boolean(true)},
      }));
  return document;
}

JsonValue YouTubeDescribeDocument() {
  JsonValue document = DomainDescribeBase(
      "youtube",
      "Search YouTube videos through the official YouTube Data API v3.",
      true,
      "youtube-data-api-v3",
      MakeArray({JsonValue::String("youtube-data-api-v3")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--query")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(true)},
          }),
          MakeObject({
              {"name", JsonValue::String("--sp")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"description",
               JsonValue::String(
                   "Legacy compatibility input. Supports empty sp, readable aliases (last-hour, today, week, month, year), or runtime-configured legacy tokens.")},
          }),
          MakeObject({
              {"name", JsonValue::String("--count")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(50)})},
              {"default", JsonValue::Integer(10)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("youtube")},
          {"source", JsonValue::String("youtube-data-api-v3")},
          {"query", JsonValue::String("string")},
          {"sp", JsonValue::String("string|null")},
          {"timeFilter", JsonValue::String("string")},
          {"count", JsonValue::String("integer")},
          {"totalResults", JsonValue::String("integer|null")},
          {"videos", JsonValue::String("array")},
      }));
  ObjectSet(
      document, "compatibility",
      MakeObject({
          {"legacy_input", JsonValue::String("query + sp")},
          {"empty_sp_behavior",
           JsonValue::String(
               "Omit time bounds and perform a plain YouTube search.")},
          {"rolling_windows",
           JsonValue::String(
               "today=24h, week=7d, month=30d, year=365d")},
          {"time_zone", JsonValue::String("Asia/Seoul")},
      }));
  ObjectSet(
      document, "environment",
      MakeObject({
          {"api_key_env",
           JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_API_KEY")},
          {"api_key_env_fallback", JsonValue::String("YOUTUBE_DATA_API_KEY")},
          {"legacy_sp_envs",
           MakeArray({
               JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_LAST_HOUR"),
               JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_TODAY"),
               JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_WEEK"),
               JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_MONTH"),
               JsonValue::String("TIZEN_TOOL_DOMAIN_FETCH_YOUTUBE_SP_YEAR"),
           })},
      }));
  return document;
}

JsonValue FinanceDescribeDocument() {
  return DomainDescribeBase(
      "finance",
      "Fetch KRW-first finance context for TV composition.",
      true,
      "naver-public",
      MakeArray(
          {JsonValue::String("mock"), JsonValue::String("naver-public")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("mock"), JsonValue::String("naver-public")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--watchlist")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default",
               JsonValue::String(
                   "005930:삼성전자,000660:SK하이닉스,035420:NAVER")},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("finance")},
          {"source", JsonValue::String("mock|naver-public")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue CommuteDescribeDocument() {
  JsonValue document = DomainDescribeBase(
      "commute",
      "Fetch route and departure recommendation context for TV composition. Both --origin and --destination are required.",
      true,
      "osrm",
      MakeArray({JsonValue::String("mock"), JsonValue::String("osrm")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("mock"), JsonValue::String("osrm")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--origin")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(true)},
              {"description",
               JsonValue::String(
                   "Required. Starting point for routing.")},
          }),
          MakeObject({
              {"name", JsonValue::String("--destination")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(true)},
              {"description",
               JsonValue::String(
                   "Required. Arrival target for routing.")},
          }),
          MakeObject({
              {"name", JsonValue::String("--profile")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("driving"), JsonValue::String("walking")})},
              {"default", JsonValue::String("driving")},
          }),
          MakeObject({
              {"name", JsonValue::String("--arrive-by")},
              {"type", JsonValue::String("ISO-8601 string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--buffer-minutes")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(0), JsonValue::Integer(180)})},
              {"default", JsonValue::Integer(8)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("commute")},
          {"source", JsonValue::String("mock|osrm")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
  ObjectSet(
      document, "input_contract",
      MakeObject({
          {"requires_origin", JsonValue::Boolean(true)},
          {"requires_destination", JsonValue::Boolean(true)},
          {"location_defaults", JsonValue::String("none")},
          {"rule",
           JsonValue::String(
               "Do not call tizen-tool-domain-fetch commute without both --origin and --destination.")},
      }));
  return document;
}

JsonValue SportsDescribeDocument() {
  return DomainDescribeBase(
      "sports",
      "Fetch league-first sports context for TV composition.",
      true,
      "thesportsdb",
      MakeArray({JsonValue::String("mock"), JsonValue::String("thesportsdb")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("mock"), JsonValue::String("thesportsdb")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--league")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("kleague1")},
          }),
          MakeObject({
              {"name", JsonValue::String("--league-id")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--league-name")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("sports")},
          {"source", JsonValue::String("mock|thesportsdb")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue ScheduleDescribeDocument() {
  return DomainDescribeBase(
      "schedule",
      "Fetch schedule briefing context from mock or live ICS feeds.",
      true,
      "ics-url",
      MakeArray({JsonValue::String("mock"),
                 JsonValue::String("ics-url"),
                 JsonValue::String("ics-file")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("mock"),
                          JsonValue::String("ics-url"),
                          JsonValue::String("ics-file")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--ics-url")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("https://holidays.hyunbin.page/basic.ics")},
          }),
          MakeObject({
              {"name", JsonValue::String("--ics-file")},
              {"type", JsonValue::String("path")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--days")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(30)})},
              {"default", JsonValue::Integer(2)},
          }),
          MakeObject({
              {"name", JsonValue::String("--max-events")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(20)})},
              {"default", JsonValue::Integer(6)},
          }),
          MakeObject({
              {"name", JsonValue::String("--now")},
              {"type", JsonValue::String("ISO-8601 string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("schedule")},
          {"source", JsonValue::String("mock|ics-url|ics-file")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue TravelDescribeDocument() {
  return DomainDescribeBase(
      "travel",
      "Fetch airport departure helper context from bundled mock data or live airport.kr feeds.",
      true,
      "airport-kr",
      MakeArray({JsonValue::String("mock"), JsonValue::String("airport-kr")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("mock"), JsonValue::String("airport-kr")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--date")},
              {"type", JsonValue::String("YYYYMMDD or ISO-8601")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--window-hours")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(24)})},
              {"default", JsonValue::Integer(4)},
          }),
          MakeObject({
              {"name", JsonValue::String("--from-time")},
              {"type", JsonValue::String("HHMM or HH:MM")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--to-time")},
              {"type", JsonValue::String("HHMM or HH:MM")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--flight-number")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--destination-code")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--terminal")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values", MakeArray({JsonValue::String("T1"), JsonValue::String("T2")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--airline")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--include-codeshare")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--now")},
              {"type", JsonValue::String("ISO-8601 string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("travel")},
          {"source", JsonValue::String("mock|airport-kr")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue EmergencyDescribeDocument() {
  return DomainDescribeBase(
      "emergency",
      "Fetch high-priority emergency context from bundled mock data or official KMA sources.",
      true,
      "kma-combined",
      MakeArray({JsonValue::String("mock"),
                 JsonValue::String("kma-special-report"),
                 JsonValue::String("kma-earthquake"),
                 JsonValue::String("kma-combined")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("mock"),
                          JsonValue::String("kma-special-report"),
                          JsonValue::String("kma-earthquake"),
                          JsonValue::String("kma-combined")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--min-magnitude")},
              {"type", JsonValue::String("number")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Double(3.0)},
          }),
          MakeObject({
              {"name", JsonValue::String("--max-age-days")},
              {"type", JsonValue::String("integer")},
              {"required", JsonValue::Boolean(false)},
              {"range", MakeArray({JsonValue::Integer(1), JsonValue::Integer(30)})},
              {"default", JsonValue::Integer(7)},
          }),
          MakeObject({
              {"name", JsonValue::String("--now")},
              {"type", JsonValue::String("ISO-8601 string")},
              {"required", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("emergency")},
          {"source", JsonValue::String("mock|kma-special-report|kma-earthquake|kma-combined")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue DailyDescribeDocument() {
  return DomainDescribeBase(
      "daily",
      "Fetch a morning digest from bundled mock data or compose-live weather, news, schedule, and commute sources.",
      true,
      "compose-live",
      MakeArray({JsonValue::String("mock"), JsonValue::String("compose-live")}),
      MakeArray({
          MakeObject({
              {"name", JsonValue::String("--source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("mock"), JsonValue::String("compose-live")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--weather-source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("open-meteo"),
                          JsonValue::String("mock"),
                          JsonValue::String("skip")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--news-source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("yonhap-rss"),
                          JsonValue::String("mock"),
                          JsonValue::String("skip")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--schedule-source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("ics-url"),
                          JsonValue::String("ics-file"),
                          JsonValue::String("mock"),
                          JsonValue::String("skip")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--commute-source")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray({JsonValue::String("osrm"),
                          JsonValue::String("mock"),
                          JsonValue::String("skip")})},
          }),
          MakeObject({
              {"name", JsonValue::String("--city")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("서울")},
          }),
          MakeObject({
              {"name", JsonValue::String("--district")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("중구")},
          }),
          MakeObject({
              {"name", JsonValue::String("--dry-run")},
              {"type", JsonValue::String("boolean")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::Boolean(false)},
          }),
          MakeObject({
              {"name", JsonValue::String("--format")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"values",
               MakeArray(
                   {JsonValue::String("json"), JsonValue::String("pretty")})},
              {"default", JsonValue::String("json")},
          }),
      }),
      MakeObject({
          {"domain", JsonValue::String("daily")},
          {"source", JsonValue::String("mock|compose-live")},
          {"title", JsonValue::String("string")},
          {"headline", JsonValue::String("string")},
          {"primaryMetrics", JsonValue::String("array")},
          {"sections", JsonValue::String("array")},
          {"alert", JsonValue::String("object")},
          {"footer", JsonValue::String("string")},
      }));
}

JsonValue SupportedTargetsArray() {
  JsonValue targets = JsonValue::Array();
  for (const auto target : kDescribeTargets) {
    ArrayAppend(targets, JsonValue::String(target));
  }
  return targets;
}

}  // namespace

std::variant<Command, AppError> ParseCommand(int argc, char** argv) {
  if (argc < 2) {
    return InvalidArguments(
        "Missing command.",
        "Use tizen-tool-domain-fetch describe, weather, news, youtube, finance, commute, sports, daily, emergency, family, meal-delivery, media, schedule, shopping, smart-home, travel, or wellness.");
  }

  const std::string_view command_name(argv[1]);
  std::vector<std::string_view> args;
  args.reserve(static_cast<std::size_t>(argc - 2));
  for (int index = 2; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  if (command_name == "describe") {
    const auto parsed = ParseDescribe(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<DescribeCommand>(parsed)};
  }
  if (command_name == "weather") {
    const auto parsed = ParseWeather(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<WeatherCommand>(parsed)};
  }
  if (command_name == "news") {
    const auto parsed = ParseNews(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<NewsCommand>(parsed)};
  }
  if (command_name == "youtube") {
    const auto parsed = ParseYouTube(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<YouTubeCommand>(parsed)};
  }
  if (command_name == "finance") {
    const auto parsed = ParseFinance(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<FinanceCommand>(parsed)};
  }
  if (command_name == "commute") {
    const auto parsed = ParseCommute(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<CommuteCommand>(parsed)};
  }
  if (command_name == "sports") {
    const auto parsed = ParseSports(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<SportsCommand>(parsed)};
  }
  if (command_name == "schedule") {
    const auto parsed = ParseSchedule(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<ScheduleCommand>(parsed)};
  }
  if (command_name == "travel") {
    const auto parsed = ParseTravel(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<TravelCommand>(parsed)};
  }
  if (command_name == "emergency") {
    const auto parsed = ParseEmergency(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<EmergencyCommand>(parsed)};
  }
  if (command_name == "daily") {
    const auto parsed = ParseDaily(args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<DailyCommand>(parsed)};
  }
  if (const auto kind = scenario::ParseKind(command_name); kind.has_value()) {
    const auto parsed = ParseScenario(*kind, args);
    if (std::holds_alternative<AppError>(parsed)) {
      return std::get<AppError>(parsed);
    }
    return Command{std::get<ScenarioCommand>(parsed)};
  }

  return InvalidArguments(
      "Unknown command.",
      "Use tizen-tool-domain-fetch describe, weather, news, youtube, finance, commute, sports, daily, emergency, family, meal-delivery, media, schedule, shopping, smart-home, travel, or wellness.");
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream << "tizen-tool-domain-fetch\n"
         << "Agent-friendly CLI for fetching normalized TV domain context.\n"
         << "Live sources are the default when available. Mock-only scenarios must use --source mock explicitly.\n\n"
         << "Commands:\n"
         << "  describe [weather|news|youtube|finance|commute|sports|schedule|travel|emergency|daily|family|meal-delivery|media|shopping|smart-home|wellness] [--format json|pretty]\n"
         << "  weather [--source mock|open-meteo] [--city ...] [--district ...]\n"
         << "          [--latitude ...] [--longitude ...] [--hours 1-24]\n"
         << "          [--dry-run] [--format json|pretty]\n"
         << "  news [--source mock|yonhap-rss|google-news-rss] [--rss-url ...]\n"
         << "       [--query ...] [--count 1-12]\n"
         << "       (no --query: latest Yonhap headlines, with --query: Google News search)\n"
         << "       [--dry-run] [--format json|pretty]\n"
         << "  youtube --query ... [--sp ...] [--count 1-50]\n"
         << "          (uses YouTube Data API v3; empty --sp means no time filter)\n"
         << "          [--dry-run] [--format json|pretty]\n"
         << "  finance [--source mock|naver-public] [--watchlist ...]\n"
          << "          [--dry-run] [--format json|pretty]\n"
         << "  commute [--source mock|osrm] [--origin ...] [--destination ...]\n"
         << "          (requires both --origin and --destination)\n"
         << "          [--profile driving|walking] [--arrive-by ...]\n"
         << "          [--buffer-minutes 0-180] [--dry-run] [--format json|pretty]\n"
         << "  sports [--source mock|thesportsdb] [--league ...] [--league-id ...]\n"
         << "         [--league-name ...] [--dry-run] [--format json|pretty]\n"
         << "  schedule [--source mock|ics-url|ics-file] [--ics-url ...] [--ics-file ...]\n"
         << "           [--days 1-30] [--max-events 1-20] [--now ...]\n"
         << "           [--dry-run] [--format json|pretty]\n"
         << "  travel [--source mock|airport-kr] [--date ...] [--window-hours 1-24]\n"
         << "         [--from-time ...] [--to-time ...] [--flight-number ...]\n"
         << "         [--destination-code ...] [--terminal T1|T2] [--airline ...]\n"
         << "         [--include-codeshare] [--now ...] [--dry-run] [--format json|pretty]\n"
         << "  emergency [--source mock|kma-special-report|kma-earthquake|kma-combined]\n"
         << "            [--min-magnitude ...] [--max-age-days 1-30] [--now ...]\n"
         << "            [--dry-run] [--format json|pretty]\n"
         << "  daily [--source mock|compose-live] [--weather-source ...]\n"
         << "        [--news-source ...] [--schedule-source ...] [--commute-source ...]\n"
         << "        [--city ...] [--district ...] [--dry-run] [--format json|pretty]\n"
          << "  family|meal-delivery|media|shopping|smart-home|wellness\n"
         << "         --source mock [--dry-run] [--format json|pretty]\n";
  return stream.str();
}

std::string_view ToString(OutputFormat format) {
  return format == OutputFormat::kPretty ? "pretty" : "json";
}

JsonValue BuildDescribeDocument(const std::optional<std::string>& target) {
  if (!target.has_value()) {
    return MakeObject({
        {"name", JsonValue::String("tizen-tool-domain-fetch")},
        {"description",
         JsonValue::String(
             "Fetch and normalize TV-ready domain context for downstream presentation composition.")},
        {"commands",
         MakeArray({
             WeatherDescribeDocument(),
             NewsDescribeDocument(),
             YouTubeDescribeDocument(),
             FinanceDescribeDocument(),
             CommuteDescribeDocument(),
             SportsDescribeDocument(),
             ScheduleDescribeDocument(),
             TravelDescribeDocument(),
             EmergencyDescribeDocument(),
             DailyDescribeDocument(),
             scenario::Describe(ScenarioCommand::Kind::kFamily),
             scenario::Describe(ScenarioCommand::Kind::kMealDelivery),
             scenario::Describe(ScenarioCommand::Kind::kMedia),
             scenario::Describe(ScenarioCommand::Kind::kShopping),
             scenario::Describe(ScenarioCommand::Kind::kSmartHome),
             scenario::Describe(ScenarioCommand::Kind::kWellness),
         })},
    });
  }

  if (*target == "weather") {
    return WeatherDescribeDocument();
  }
  if (*target == "news") {
    return NewsDescribeDocument();
  }
  if (*target == "youtube") {
    return YouTubeDescribeDocument();
  }
  if (*target == "finance") {
    return FinanceDescribeDocument();
  }
  if (*target == "commute") {
    return CommuteDescribeDocument();
  }
  if (*target == "sports") {
    return SportsDescribeDocument();
  }
  if (*target == "schedule") {
    return ScheduleDescribeDocument();
  }
  if (*target == "travel") {
    return TravelDescribeDocument();
  }
  if (*target == "emergency") {
    return EmergencyDescribeDocument();
  }
  if (*target == "daily") {
    return DailyDescribeDocument();
  }
  if (const auto kind = scenario::ParseKind(*target); kind.has_value()) {
    return scenario::Describe(*kind);
  }

  return MakeObject({
      {"name", JsonValue::String("tizen-tool-domain-fetch")},
      {"warning", JsonValue::String("Unknown describe target.")},
      {"supported_targets", SupportedTargetsArray()},
  });
}

}  // namespace tizen_tool_domain_fetch
