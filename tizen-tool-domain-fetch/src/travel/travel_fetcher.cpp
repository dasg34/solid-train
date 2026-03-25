#include "tizen_tool_domain_fetch/travel/travel_fetcher.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::travel {

namespace {

constexpr std::string_view kDepartureListUrl =
    "https://airport.kr/dep/ap_ko/getDepPasSchList.do";
constexpr std::string_view kDepartureDetailUrl =
    "https://airport.kr/dep/ap_ko/depPasSchDetail.do";
constexpr std::string_view kCongestionUrl =
    "https://www.airport.kr/ap_ko/883/subview.do";

struct LocalDateTime {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

struct CongestionSummary {
  std::vector<std::pair<std::string, int>> rows;
  std::string terminal;
  std::string date;
};

AppError MakeTravelError(std::string code,
                         std::string message,
                         std::string hint,
                         int exit_code = 6) {
  return AppError{std::move(code), std::move(message), std::move(hint),
                  exit_code};
}

LocalDateTime NowLocal() {
  const std::time_t now = std::time(nullptr);
  const std::tm* local = std::localtime(&now);
  if (local == nullptr) {
    throw MakeTravelError("travel_time_failed",
                          "Failed to resolve local time.",
                          "std::localtime returned null.");
  }
  return LocalDateTime{local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                       local->tm_hour, local->tm_min, local->tm_sec};
}

std::time_t ToTimeT(const LocalDateTime& value) {
  std::tm tm = {};
  tm.tm_year = value.year - 1900;
  tm.tm_mon = value.month - 1;
  tm.tm_mday = value.day;
  tm.tm_hour = value.hour;
  tm.tm_min = value.minute;
  tm.tm_sec = value.second;
  tm.tm_isdst = -1;
  const std::time_t timestamp = std::mktime(&tm);
  if (timestamp == static_cast<std::time_t>(-1)) {
    throw MakeTravelError("travel_time_failed",
                          "Failed to normalize date time.",
                          "std::mktime returned -1.");
  }
  return timestamp;
}

LocalDateTime FromTimeT(std::time_t timestamp) {
  const std::tm* local = std::localtime(&timestamp);
  if (local == nullptr) {
    throw MakeTravelError("travel_time_failed",
                          "Failed to materialize date time.",
                          "std::localtime returned null.");
  }
  return LocalDateTime{local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                       local->tm_hour, local->tm_min, local->tm_sec};
}

LocalDateTime ParseIsoLocal(std::string_view value,
                            const LocalDateTime& fallback) {
  if (value.empty()) {
    return fallback;
  }
  LocalDateTime parsed = {};
  try {
    if (value.size() < 16) {
      throw std::runtime_error("short");
    }
    parsed.year = std::stoi(std::string(value.substr(0, 4)));
    parsed.month = std::stoi(std::string(value.substr(5, 2)));
    parsed.day = std::stoi(std::string(value.substr(8, 2)));
    parsed.hour = std::stoi(std::string(value.substr(11, 2)));
    parsed.minute = std::stoi(std::string(value.substr(14, 2)));
    parsed.second = 0;
    if (value.size() >= 19 && value[16] == ':') {
      parsed.second = std::stoi(std::string(value.substr(17, 2)));
    }
  } catch (...) {
    throw MakeTravelError("travel_invalid_time",
                          "Time arguments must use ISO-8601 format.",
                          std::string(value));
  }
  return parsed;
}

LocalDateTime AddMinutes(const LocalDateTime& value, int minutes) {
  return FromTimeT(ToTimeT(value) + static_cast<std::time_t>(minutes) * 60);
}

std::string FormatHhmm(const LocalDateTime& value) {
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(2) << value.hour << ":"
         << std::setw(2) << value.minute;
  return stream.str();
}

std::string FormatHhmm(std::time_t timestamp) {
  return FormatHhmm(FromTimeT(timestamp));
}

std::string RenderDateToken(const LocalDateTime& value) {
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(4) << value.year << std::setw(2)
         << value.month << std::setw(2) << value.day;
  return stream.str();
}

std::string ParseDateToken(std::string_view value, const LocalDateTime& fallback) {
  if (value.empty()) {
    return RenderDateToken(fallback);
  }
  const std::string stripped = CleanText(value, 32);
  if (std::regex_match(stripped, std::regex(R"(\d{8})"))) {
    return stripped;
  }
  return RenderDateToken(ParseIsoLocal(stripped, fallback));
}

std::string NormalizeHhmm(std::string_view value) {
  std::string stripped;
  for (unsigned char ch : value) {
    if (std::isspace(ch) != 0 || ch == ':') {
      continue;
    }
    stripped.push_back(static_cast<char>(ch));
  }
  if (!std::regex_match(stripped, std::regex(R"(\d{4})"))) {
    throw MakeTravelError("travel_invalid_time",
                          "Time values must be HHMM or HH:MM.",
                          std::string(value));
  }
  const int hour = std::stoi(stripped.substr(0, 2));
  const int minute = std::stoi(stripped.substr(2, 2));
  if (hour > 23 || minute > 59) {
    throw MakeTravelError("travel_invalid_time",
                          "Time values must be valid 24-hour timestamps.",
                          stripped);
  }
  return stripped;
}

std::pair<std::string, std::string> DeriveTimeWindow(const TravelCommand& command,
                                                     const LocalDateTime& now,
                                                     bool prefer_full_day) {
  if (!command.from_time.empty() && !command.to_time.empty()) {
    return {NormalizeHhmm(command.from_time), NormalizeHhmm(command.to_time)};
  }
  if (prefer_full_day && command.from_time.empty() && command.to_time.empty()) {
    return {"0000", "2359"};
  }
  std::ostringstream from_stream;
  from_stream << std::setfill('0') << std::setw(2) << now.hour << std::setw(2)
              << now.minute;
  const LocalDateTime to = AddMinutes(now, std::max(1, command.window_hours) * 60);
  std::ostringstream to_stream;
  to_stream << std::setfill('0') << std::setw(2) << to.hour << std::setw(2)
            << to.minute;

  if (!command.from_time.empty()) {
    return {NormalizeHhmm(command.from_time), to_stream.str()};
  }
  if (!command.to_time.empty()) {
    return {from_stream.str(), NormalizeHhmm(command.to_time)};
  }
  return {from_stream.str(), to_stream.str()};
}

std::string JsonString(const JsonValue& object, std::string_view key,
                       std::string_view fallback = {}) {
  return object.At(key).AsString(fallback);
}

std::string RegexFirst(std::string_view text, const std::regex& pattern) {
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return "";
  }
  return std::string(match[1].first, match[1].second);
}

std::string BuildAirportForm(const std::vector<std::pair<std::string, std::string>>& pairs) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < pairs.size(); ++index) {
    if (index > 0) {
      stream << "&";
    }
    stream << UrlEncode(pairs[index].first) << "=" << UrlEncode(pairs[index].second);
  }
  return stream.str();
}

JsonValue AirportPostJson(std::string_view url, std::string_view form_body) {
  const auto response = HttpPostForm(url, form_body);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "travel_airport_request_failed";
    error.message = "Airport live request failed.";
    error.hint = std::string(url) + " | " + error.hint;
    throw error;
  }

  auto parsed = JsonValue::Parse(std::get<std::string>(response),
                                 "travel_parse_failed",
                                 "Airport live response could not be parsed.",
                                 6);
  if (std::holds_alternative<AppError>(parsed)) {
    throw std::get<AppError>(parsed);
  }
  return std::get<JsonValue>(std::move(parsed));
}

std::vector<JsonValue> FetchDepartureSchedule(const TravelCommand& command,
                                              const std::string& date,
                                              const std::string& from_hhmm,
                                              const std::string& to_hhmm,
                                              const LocalDateTime& now) {
  std::vector<std::pair<std::string, std::string>> pairs = {
      {"siteId", "ap_ko"},
      {"langSe", "ko"},
      {"daySel", date},
      {"todayDate", RenderDateToken(now)},
      {"tomorrowDate", RenderDateToken(AddMinutes(now, 24 * 60))},
      {"todayTime", NormalizeHhmm(FormatHhmm(now))},
      {"curDate", date},
      {"curStime", from_hhmm},
      {"curEtime", to_hhmm},
      {"fromTime", from_hhmm},
      {"toTime", to_hhmm},
      {"page", "1"},
      {"row", "100"},
      {"arrOrDep", "D"},
      {"porc", "P"},
      {"intg", ""},
      {"keyWord", ""},
  };
  if (!command.terminal.empty()) {
    pairs.push_back({"termId", command.terminal});
  }

  JsonValue payload = AirportPostJson(kDepartureListUrl, BuildAirportForm(pairs));
  JsonValue schedules = payload.At("scheduleList");
  if (!schedules.IsArray()) {
    throw MakeTravelError("travel_invalid_response",
                          "Airport departure response did not include scheduleList.",
                          std::string(kDepartureListUrl));
  }

  std::vector<JsonValue> items;
  for (std::size_t index = 0; index < schedules.Size(); ++index) {
    JsonValue item = schedules.At(index);
    if (item.IsObject()) {
      items.push_back(item);
    }
  }
  return items;
}

bool IsCodeshareSlave(const JsonValue& schedule) {
  return CleanText(JsonString(schedule, "codeshare"), 16) == "slave";
}

bool ScheduleMatches(const JsonValue& schedule, const TravelCommand& command) {
  if (!command.terminal.empty() &&
      CleanText(JsonString(schedule, "terminal"), 4) != command.terminal) {
    return false;
  }
  if (!command.destination_code.empty() &&
      CleanText(JsonString(schedule, "p1code"), 8) != command.destination_code) {
    return false;
  }
  if (!command.airline.empty() &&
      CleanText(JsonString(schedule, "flightCarrier"), 8) != command.airline) {
    return false;
  }
  if (!command.flight_number.empty()) {
    const std::string target = CleanText(command.flight_number, 16);
    const std::vector<std::string> candidates = {
        CleanText(JsonString(schedule, "fnumber"), 16),
        CleanText(JsonString(schedule, "masterflight"), 16),
        CleanText(JsonString(schedule, "codeshareFlight"), 16),
    };
    if (std::find(candidates.begin(), candidates.end(), target) ==
        candidates.end()) {
      return false;
    }
  }
  if (!command.include_codeshare && IsCodeshareSlave(schedule) &&
      command.flight_number.empty()) {
    return false;
  }
  return true;
}

std::optional<std::time_t> ParseDepartureTimestamp(std::string_view value,
                                                   std::string_view fallback_date) {
  const std::string stripped = CleanText(value, 24);
  if (stripped.empty()) {
    return std::nullopt;
  }

  std::tm tm = {};
  {
    std::stringstream stream(stripped);
    stream >> std::get_time(&tm, "%Y%m%d%H%M");
    if (!stream.fail()) {
      tm.tm_isdst = -1;
      return std::mktime(&tm);
    }
  }
  {
    std::stringstream stream(stripped);
    stream >> std::get_time(&tm, "%Y.%m.%d %H:%M");
    if (!stream.fail()) {
      tm.tm_isdst = -1;
      return std::mktime(&tm);
    }
  }
  if (std::regex_match(stripped, std::regex(R"(\d{2}:\d{2})"))) {
    const std::string combined = std::string(fallback_date) + stripped.substr(0, 2) +
                                 stripped.substr(3, 2);
    std::stringstream stream(combined);
    stream >> std::get_time(&tm, "%Y%m%d%H%M");
    if (!stream.fail()) {
      tm.tm_isdst = -1;
      return std::mktime(&tm);
    }
  }
  return std::nullopt;
}

std::tuple<int, std::time_t, std::string> SchedulePriority(const JsonValue& schedule,
                                                           const LocalDateTime& now,
                                                           std::string_view date) {
  const std::string status = CleanText(JsonString(schedule, "stattxt"), 16);
  const auto departure = ParseDepartureTimestamp(JsonString(schedule, "etime"), date);
  const auto actual = ParseDepartureTimestamp(JsonString(schedule, "btime"), date);
  const std::time_t now_ts = ToTimeT(now);
  const std::time_t effective = actual.value_or(departure.value_or(now_ts));

  int bucket = 0;
  if (status == "출발") {
    bucket = 4;
  } else if (status == "탑승마감") {
    bucket = 3;
  } else if (effective < now_ts - 20 * 60) {
    bucket = 2;
  } else if (effective < now_ts) {
    bucket = 1;
  }
  return {bucket, effective, CleanText(JsonString(schedule, "fnumber"), 16)};
}

int FlightMatchRank(const JsonValue& schedule, std::string_view flight_number) {
  if (flight_number.empty()) {
    return 0;
  }
  const std::string target = CleanText(flight_number, 16);
  if (CleanText(JsonString(schedule, "fnumber"), 16) == target) {
    return 0;
  }
  if (CleanText(JsonString(schedule, "masterflight"), 16) == target) {
    return 1;
  }
  if (CleanText(JsonString(schedule, "codeshareFlight"), 16) == target) {
    return 2;
  }
  return 3;
}

std::optional<JsonValue> ChooseSchedule(std::vector<JsonValue> schedules,
                                        const TravelCommand& command,
                                        const LocalDateTime& now,
                                        std::string_view date) {
  schedules.erase(std::remove_if(schedules.begin(), schedules.end(),
                                 [&](const JsonValue& schedule) {
                                   return !ScheduleMatches(schedule, command);
                                 }),
                  schedules.end());
  if (schedules.empty()) {
    return std::nullopt;
  }

  std::sort(schedules.begin(), schedules.end(),
            [&](const JsonValue& left, const JsonValue& right) {
              return std::make_tuple(FlightMatchRank(left, command.flight_number),
                                     SchedulePriority(left, now, date)) <
                     std::make_tuple(FlightMatchRank(right, command.flight_number),
                                     SchedulePriority(right, now, date));
            });
  return schedules.front();
}

JsonValue FetchDepartureDetail(const JsonValue& schedule) {
  const std::string afs_id = CleanText(JsonString(schedule, "afsId"), 40);
  const std::string airport_code = CleanText(JsonString(schedule, "p1code"), 8);
  if (afs_id.empty() || airport_code.empty()) {
    throw MakeTravelError("travel_invalid_response",
                          "Selected flight did not include detail query fields.",
                          "afsId / airportCode missing.");
  }
  return AirportPostJson(
      kDepartureDetailUrl,
      BuildAirportForm({
          {"afsId", afs_id},
          {"airportCode", airport_code},
      }));
}

std::vector<std::pair<std::string, int>> ParseCongestionRows(std::string_view html) {
  const std::string tbody = RegexFirst(
      html,
      std::regex(R"(<table id="userEx"[\s\S]*?<tbody>([\s\S]*?)</tbody>)"));
  if (tbody.empty()) {
    throw MakeTravelError("travel_congestion_failed",
                          "Failed to locate the airport congestion table.",
                          std::string(kCongestionUrl));
  }

  std::vector<std::pair<std::string, int>> rows;
  const std::regex row_pattern(R"(<tr[^>]*>\s*<th>([^<]+)</th>([\s\S]*?)</tr>)");
  const std::regex cell_pattern(R"(<td[^>]*>([\s\S]*?)</td>)");
  for (std::sregex_iterator row_iter(tbody.begin(), tbody.end(), row_pattern), end;
       row_iter != end; ++row_iter) {
    std::vector<std::string> totals;
    const std::string cells_html = (*row_iter)[2].str();
    for (std::sregex_iterator cell_iter(cells_html.begin(), cells_html.end(), cell_pattern);
         cell_iter != end; ++cell_iter) {
      const std::string cell = CleanText(
          std::regex_replace((*cell_iter)[1].str(), std::regex("<[^>]+>"), " "), 40);
      if (std::regex_match(cell, std::regex(R"(\d+)"))) {
        totals.push_back(cell);
      }
    }
    if (totals.empty()) {
      continue;
    }
    rows.emplace_back(CleanText((*row_iter)[1].str(), 16),
                      std::stoi(totals.back()));
  }
  return rows;
}

std::optional<CongestionSummary> FetchCongestionSummary(std::string_view date,
                                                        std::string_view terminal) {
  const std::string url = std::string(kCongestionUrl) + "?selTm=" +
                          UrlEncode(terminal) + "&pday=" + UrlEncode(date);
  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    return std::nullopt;
  }

  try {
    const auto rows = ParseCongestionRows(std::get<std::string>(response));
    if (rows.empty()) {
      return std::nullopt;
    }
    return CongestionSummary{rows, std::string(terminal), std::string(date)};
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::pair<std::string, int>> FindCongestionRow(
    const CongestionSummary& summary, int target_hour) {
  std::ostringstream desired;
  desired << std::setfill('0') << std::setw(2) << target_hour << "~"
          << std::setw(2) << ((target_hour + 1) % 24) << "시";
  for (const auto& row : summary.rows) {
    if (row.first.rfind(desired.str(), 0) == 0) {
      return row;
    }
  }
  if (summary.rows.empty()) {
    return std::nullopt;
  }
  return summary.rows.front();
}

std::pair<std::string, std::string> CongestionLevel(int total) {
  if (total >= 4000) {
    return {"높음", "출국장 혼잡 가능"};
  }
  if (total >= 2200) {
    return {"보통", "혼잡 대비 필요"};
  }
  return {"낮음", "비교적 원활"};
}

std::string FormatCountdown(std::time_t target, std::time_t now) {
  const int delta_minutes = static_cast<int>(std::llround(
      std::difftime(target, now) / 60.0));
  if (delta_minutes <= 0) {
    const int overdue = std::abs(delta_minutes);
    if (overdue <= 2) {
      return "지금";
    }
    return std::to_string(overdue) + "분 경과";
  }
  const int hours = delta_minutes / 60;
  const int minutes = delta_minutes % 60;
  if (hours > 0 && minutes > 0) {
    return std::to_string(hours) + "시간 " + std::to_string(minutes) + "분";
  }
  if (hours > 0) {
    return std::to_string(hours) + "시간";
  }
  return std::to_string(minutes) + "분";
}

std::string FormatClockText(std::string_view value) {
  static const std::regex pattern(R"((\d{2}:\d{2}))");
  std::smatch match;
  const std::string text = CleanText(value, 20);
  if (std::regex_search(text, match, pattern) && match.size() >= 2) {
    return match[1].str();
  }
  return text.empty() ? "-" : text;
}

std::string FormatTemperature(std::string_view value) {
  try {
    return std::to_string(static_cast<int>(std::lround(std::stod(std::string(value))))) + "°";
  } catch (...) {
    return "-";
  }
}

std::string FormatElapsedTime(std::string_view value) {
  const std::string text = CleanText(value, 8);
  if (!std::regex_match(text, std::regex(R"(\d{4})"))) {
    return "";
  }
  const int hour = std::stoi(text.substr(0, 2));
  const int minute = std::stoi(text.substr(2, 2));
  if (hour > 0 && minute > 0) {
    return std::to_string(hour) + "시간 " + std::to_string(minute) + "분";
  }
  if (hour > 0) {
    return std::to_string(hour) + "시간";
  }
  return std::to_string(minute) + "분";
}

std::string DetailText(std::initializer_list<std::string> parts) {
  std::ostringstream stream;
  bool first = true;
  for (const auto& part : parts) {
    if (part.empty()) {
      continue;
    }
    if (!first) {
      stream << " · ";
    }
    stream << part;
    first = false;
  }
  return CleanText(stream.str(), 72);
}

JsonValue NormalizeAirportTravel(const JsonValue& schedule,
                                 const JsonValue& detail,
                                 const std::optional<CongestionSummary>& congestion,
                                 const LocalDateTime& now,
                                 std::string_view date) {
  const JsonValue view_info = detail.At("viewInfo");
  const JsonValue city_info = detail.At("cityInfo");
  const JsonValue weather_info = detail.At("weatherList").At(0);
  const JsonValue airport_info = detail.At("airportInfo");

  const auto scheduled_departure =
      ParseDepartureTimestamp(JsonString(schedule, "etime"), date);
  const auto actual_departure =
      ParseDepartureTimestamp(JsonString(schedule, "btime"), date);
  const std::time_t now_ts = ToTimeT(now);
  const std::time_t current_departure =
      actual_departure.value_or(scheduled_departure.value_or(now_ts));

  const std::string status =
      CleanText(JsonString(schedule, "stattxt", "운항 정보"), 16);
  const std::string terminal = CleanText(
                                   JsonString(view_info, "terminal",
                                              JsonString(schedule, "terminal", "T1")),
                                   4)
                                   .empty()
                               ? "T1"
                               : CleanText(JsonString(view_info, "terminal",
                                                      JsonString(schedule, "terminal", "T1")),
                                           4);
  const std::string gate =
      CleanText(JsonString(view_info, "gatenumber", JsonString(schedule, "gatenumber", "-")),
                12);
  const std::string flight_number = CleanText(JsonString(schedule, "fnumber"), 16);
  const std::string destination = CleanText(
      JsonString(schedule, "airportName1", JsonString(view_info, "airportNameKo")),
      24);
  const std::string airport_code = CleanText(JsonString(schedule, "p1code"), 8);
  const std::string counter = CleanText(
                                  JsonString(view_info, "airCounter",
                                             JsonString(schedule, "chkinrange", "-")),
                                  20)
                                  .empty()
                              ? "-"
                              : CleanText(JsonString(view_info, "airCounter",
                                                     JsonString(schedule, "chkinrange", "-")),
                                          20);
  const std::string local_time = FormatClockText(JsonString(detail, "timeZoneHour"));
  const std::string local_date = CleanText(JsonString(detail, "timeZoneDate"), 24);
  const std::string move_time = CleanText(JsonString(view_info, "moveTimeKo", "-"), 20);
  const std::string destination_weather =
      CleanText(JsonString(weather_info, "weather"), 16);
  const std::string destination_temp =
      FormatTemperature(JsonString(weather_info, "temp"));
  const std::string elapsed = FormatElapsedTime(
      JsonString(schedule, "elapseTime", JsonString(airport_info, "elapseTime")));

  std::string congestion_label = "확인 필요";
  std::string congestion_detail = "공항 혼잡도 표 확인";
  std::string congestion_item_detail = "공항 혼잡도 예고를 읽지 못했습니다.";
  if (congestion.has_value()) {
    const auto target_row = FindCongestionRow(*congestion, FromTimeT(current_departure).hour);
    if (target_row.has_value()) {
      const auto [level, level_detail] = CongestionLevel(target_row->second);
      congestion_label = level;
      congestion_detail =
          CleanText(target_row->first + " " + std::to_string(target_row->second) + "명 예고", 32);
      congestion_item_detail = DetailText(
          {target_row->first, std::to_string(target_row->second) + "명 예고", level_detail});
    }
  }

  JsonValue metrics = JsonValue::Array();
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("탑승까지")},
                  {"value", JsonValue::String(FormatCountdown(current_departure, now_ts))},
                  {"detail",
                   JsonValue::String(DetailText({"T" + terminal.substr(1), "게이트 " + gate, status}))},
              }));
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("체크인")},
                  {"value", JsonValue::String(counter)},
                  {"detail", JsonValue::String(DetailText({"출국심사 후 이동", move_time}))},
              }));
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("출국 혼잡")},
                  {"value", JsonValue::String(congestion_label)},
                  {"detail", JsonValue::String(congestion_detail)},
              }));

  JsonValue sections = JsonValue::Array();
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("항공편")},
          {"items",
           MakeArray({
               MakeObject({
                   {"icon", JsonValue::String("flightTakeoff")},
                   {"label", JsonValue::String(flight_number)},
                   {"value", JsonValue::String(FormatHhmm(current_departure) + " 출발")},
                   {"detail", JsonValue::String(DetailText({destination, airport_code, status}))},
               }),
               MakeObject({
                   {"icon", JsonValue::String("place")},
                   {"label", JsonValue::String("게이트")},
                   {"value", JsonValue::String(gate.empty() ? "-" : gate)},
                   {"detail", JsonValue::String(DetailText({terminal, "체크인 " + counter}))},
               }),
           })},
      }));
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("체크포인트")},
          {"items",
           MakeArray({
               MakeObject({
                   {"icon", JsonValue::String("security")},
                   {"label", JsonValue::String("출국 후 이동")},
                   {"value", JsonValue::String(move_time.empty() ? "-" : move_time)},
                   {"detail", JsonValue::String("주차장-연결통로-체크인-탑승구 순서 · 공항 내부 기준")},
               }),
               MakeObject({
                   {"icon", JsonValue::String("groups")},
                   {"label", JsonValue::String("출국장 혼잡")},
                   {"value", JsonValue::String(congestion_label)},
                   {"detail", JsonValue::String(congestion_item_detail)},
               }),
           })},
      }));
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("목적지")},
          {"items",
           MakeArray({
               MakeObject({
                   {"icon", JsonValue::String("schedule")},
                   {"label", JsonValue::String("현지 시각")},
                   {"value", JsonValue::String(local_time)},
                   {"detail", JsonValue::String(local_date.empty() ? "현지 날짜 확인 필요" : local_date)},
               }),
               MakeObject({
                   {"icon", JsonValue::String("wbSunny")},
                   {"label", JsonValue::String("도착지 날씨")},
                   {"value",
                    JsonValue::String(CleanText(destination_weather + " " + destination_temp, 24))},
                   {"detail",
                    JsonValue::String(DetailText({
                        elapsed.empty() ? "" : "비행 " + elapsed,
                        CleanText(JsonString(city_info, "countryNameKo"), 16),
                    }))},
               }),
           })},
      }));

  JsonValue alert = JsonValue::Object();
  if (status == "지연") {
    alert = MakeObject({
        {"icon", JsonValue::String("warning")},
        {"title", JsonValue::String("운항 상태 변경됨")},
        {"summary",
         JsonValue::String(CleanText(
             flight_number + "편이 지연 상태입니다. 항공사 앱이나 공항 전광판에서 최종 출발 시각을 다시 확인해 주세요.",
             100))},
        {"meta", JsonValue::String(CleanText(terminal + " · 게이트 " + gate, 32))},
    });
  } else if (status == "탑승마감") {
    alert = MakeObject({
        {"icon", JsonValue::String("update")},
        {"title", JsonValue::String("탑승 마감 상태")},
        {"summary",
         JsonValue::String(CleanText(
             flight_number + "편은 탑승마감 상태로 표시됩니다. 동행자 확인용 화면이라면 새 운항편을 다시 선택하는 편이 안전합니다.",
             100))},
        {"meta", JsonValue::String(CleanText(terminal + " · 게이트 " + gate, 32))},
    });
  } else {
    alert = MakeObject({
        {"icon", JsonValue::String("update")},
        {"title", JsonValue::String("운영 정보 변동 가능")},
        {"summary",
         JsonValue::String("게이트, 출발시각, 출국장 혼잡도는 짧은 시간에도 바뀔 수 있으니 마지막 조회 시각을 함께 유지하는 편이 안전합니다.")},
        {"meta", JsonValue::String("인천공항 공식 출발편")},
    });
  }

  return MakeObject({
      {"domain", JsonValue::String("travel")},
      {"source", JsonValue::String("airport-kr")},
      {"title", JsonValue::String("공항 출발 어시스턴트")},
      {"headline",
       JsonValue::String(CleanText(
           flight_number + " " + destination + "행 " + FormatHhmm(current_departure) +
               " 출발 기준으로 게이트와 공항 체크포인트를 정리했습니다.",
           88))},
      {"primaryMetrics", std::move(metrics)},
      {"sections", std::move(sections)},
      {"alert", std::move(alert)},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshTravel")},
           }),
           MakeObject({
               {"label", JsonValue::String("다른 운항편")},
               {"event", JsonValue::String("showTravelFlights")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "live adapter는 인천공항 공식 출발편과 공항 예상 혼잡도 페이지를 사용합니다. 예약번호 대신 운항편명, 터미널, 게이트 중심으로 요약하는 편이 TV에 적합합니다.")},
  });
}

JsonValue EmptyTravelPayload() {
  return MakeObject({
      {"domain", JsonValue::String("travel")},
      {"source", JsonValue::String("airport-kr")},
      {"status", JsonValue::String("empty")},
      {"title", JsonValue::String("표시할 출발편 없음")},
      {"headline", JsonValue::String("현재 조건에 맞는 공항 출발 정보가 없습니다.")},
      {"primaryMetrics", JsonValue::Array()},
      {"sections", JsonValue::Array()},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("info")},
           {"title", JsonValue::String("검색 결과 없음")},
           {"summary", JsonValue::String("항공편 번호, 날짜, 터미널, 시간 창을 다시 확인해 주세요.")},
           {"meta", JsonValue::String("인천공항 출발편")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshTravel")},
           }),
       })},
      {"footer", JsonValue::String("검색 조건에 맞는 출발편이 없으면 empty payload를 반환합니다.")},
  });
}

}  // namespace

JsonResult LoadMockTravelPayload() {
  return LoadFixturePayload("mock_travel.json", "travel", "mock");
}

JsonResult Execute(const TravelCommand& command) {
  if (command.source == TravelCommand::Source::kMock) {
    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("travel")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           JsonValue::String(ResolveFixturePath("mock_travel.json").string())},
      });
    }
    return LoadMockTravelPayload();
  }

  try {
    const LocalDateTime now = ParseIsoLocal(command.now, NowLocal());
    const std::string date = ParseDateToken(command.date, now);
    const auto [from_hhmm, to_hhmm] = DeriveTimeWindow(
        command, now,
        !command.flight_number.empty() || !command.destination_code.empty() ||
            !command.airline.empty());

    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("travel")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("airport-kr")},
          {"date", JsonValue::String(date)},
          {"from_time", JsonValue::String(from_hhmm)},
          {"to_time", JsonValue::String(to_hhmm)},
          {"departure_list_url", JsonValue::String(kDepartureListUrl)},
          {"departure_detail_url", JsonValue::String(kDepartureDetailUrl)},
      });
    }

    const auto schedules =
        FetchDepartureSchedule(command, date, from_hhmm, to_hhmm, now);
    const auto selected = ChooseSchedule(schedules, command, now, date);
    if (!selected.has_value()) {
      return EmptyTravelPayload();
    }

    const JsonValue detail = FetchDepartureDetail(*selected);
    const std::string terminal = CleanText(
        JsonString(detail.At("viewInfo"), "terminal",
                   JsonString(*selected, "terminal",
                              command.terminal.empty() ? "T1" : command.terminal)),
        4);
    const auto congestion = FetchCongestionSummary(date, terminal.empty() ? "T1" : terminal);
    return NormalizeAirportTravel(*selected, detail, congestion, now, date);
  } catch (const AppError& error) {
    return error;
  } catch (const std::exception& error) {
    return MakeTravelError("travel_live_failed",
                           "Failed to build the live travel payload.",
                           CleanText(error.what(), 120));
  }
}

}  // namespace tizen_tool_domain_fetch::travel
