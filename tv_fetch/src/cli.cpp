#include "tv_fetch/cli.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

#include "tv_fetch/support.hpp"

namespace tv_fetch {

namespace {

constexpr std::array<std::string_view, 5> kDescribeTargets = {
    "weather",
    "news",
    "finance",
    "commute",
    "sports",
};

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
  if (index < args.size() && !args[index].starts_with("--")) {
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
        "Use tv_fetch describe [weather|news|finance|commute|sports] [--format json|pretty].");
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
        "Use tv_fetch weather [--source mock|open-meteo] [--city ...] "
        "[--district ...] [--latitude ...] [--longitude ...] [--hours ...] "
        "[--dry-run] [--format json|pretty].");
  }

  if (command.latitude.has_value() != command.longitude.has_value()) {
    return InvalidArguments(
        "Latitude and longitude must be provided together.",
        "Either provide both --latitude and --longitude, or omit both to use geocoding.");
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
      if (!source_explicit && command.source == NewsCommand::Source::kMock) {
        command.source = NewsCommand::Source::kGoogleNewsRss;
      } else if (source_explicit &&
                 command.source != NewsCommand::Source::kGoogleNewsRss) {
        return InvalidArguments(
            "News search requires the google-news-rss source.",
            "Use tv_fetch news --query ... or explicitly set --source google-news-rss.");
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
        "Use tv_fetch news [--source mock|yonhap-rss|google-news-rss] "
        "[--rss-url ...] [--query ...] [--count 1-12] [--dry-run] "
        "[--format json|pretty].");
  }

  if (command.source == NewsCommand::Source::kGoogleNewsRss &&
      command.query.empty()) {
    return InvalidArguments(
        "Google News search requires a query.",
        "Use tv_fetch news --query 반도체 [--count ...] [--format pretty].");
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
        "Use tv_fetch finance [--source mock|naver-public] [--watchlist ...] "
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
        "Use tv_fetch commute [--source mock|osrm] [--origin ...] "
        "[--destination ...] [--origin-label ...] [--destination-label ...] "
        "[--profile driving|walking] [--arrive-by ...] [--now ...] "
        "[--buffer-minutes 0-180] [--dry-run] [--format json|pretty].");
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
        "Use tv_fetch sports [--source mock|thesportsdb] [--league ...] "
        "[--league-id ...] [--league-name ...] [--dry-run] "
        "[--format json|pretty].");
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
      "mock",
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
              {"default", JsonValue::String("서울")},
          }),
          MakeObject({
              {"name", JsonValue::String("--district")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("중구")},
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
  return DomainDescribeBase(
      "news",
      "Fetch headline-first news context for TV composition.",
      true,
      "mock",
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
                   "Enables Google News RSS search when provided.")},
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
}

JsonValue FinanceDescribeDocument() {
  return DomainDescribeBase(
      "finance",
      "Fetch KRW-first finance context for TV composition.",
      true,
      "mock",
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
  return DomainDescribeBase(
      "commute",
      "Fetch route and departure recommendation context for TV composition.",
      true,
      "mock",
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
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("서울시청")},
          }),
          MakeObject({
              {"name", JsonValue::String("--destination")},
              {"type", JsonValue::String("string")},
              {"required", JsonValue::Boolean(false)},
              {"default", JsonValue::String("강남역")},
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
}

JsonValue SportsDescribeDocument() {
  return DomainDescribeBase(
      "sports",
      "Fetch league-first sports context for TV composition.",
      true,
      "mock",
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
        "Use tv_fetch describe, weather, news, finance, commute, or sports.");
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

  return InvalidArguments(
      "Unknown command.",
      "Use tv_fetch describe, weather, news, finance, commute, or sports.");
}

std::string RenderHelp() {
  std::ostringstream stream;
  stream << "tv_fetch\n"
         << "Agent-friendly CLI for fetching normalized TV domain context.\n\n"
         << "Commands:\n"
         << "  describe [weather|news|finance|commute|sports] [--format json|pretty]\n"
         << "  weather [--source mock|open-meteo] [--city ...] [--district ...]\n"
         << "          [--latitude ...] [--longitude ...] [--hours 1-24]\n"
         << "          [--dry-run] [--format json|pretty]\n"
         << "  news [--source mock|yonhap-rss|google-news-rss] [--rss-url ...]\n"
         << "       [--query ...] [--count 1-12]\n"
         << "       [--dry-run] [--format json|pretty]\n"
         << "  finance [--source mock|naver-public] [--watchlist ...]\n"
         << "          [--dry-run] [--format json|pretty]\n"
         << "  commute [--source mock|osrm] [--origin ...] [--destination ...]\n"
         << "          [--profile driving|walking] [--arrive-by ...]\n"
         << "          [--buffer-minutes 0-180] [--dry-run] [--format json|pretty]\n"
         << "  sports [--source mock|thesportsdb] [--league ...] [--league-id ...]\n"
         << "         [--league-name ...] [--dry-run] [--format json|pretty]\n";
  return stream.str();
}

std::string_view ToString(OutputFormat format) {
  return format == OutputFormat::kPretty ? "pretty" : "json";
}

JsonValue BuildDescribeDocument(const std::optional<std::string>& target) {
  if (!target.has_value()) {
    return MakeObject({
        {"name", JsonValue::String("tv_fetch")},
        {"description",
         JsonValue::String(
             "Fetch and normalize TV-ready domain context for downstream A2UI composition.")},
        {"commands",
         MakeArray({
             WeatherDescribeDocument(),
             NewsDescribeDocument(),
             FinanceDescribeDocument(),
             CommuteDescribeDocument(),
             SportsDescribeDocument(),
         })},
    });
  }

  if (*target == "weather") {
    return WeatherDescribeDocument();
  }
  if (*target == "news") {
    return NewsDescribeDocument();
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

  return MakeObject({
      {"name", JsonValue::String("tv_fetch")},
      {"warning", JsonValue::String("Unknown describe target.")},
      {"supported_targets", SupportedTargetsArray()},
  });
}

}  // namespace tv_fetch
