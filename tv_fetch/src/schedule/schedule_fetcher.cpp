#include "tv_fetch/schedule/schedule_fetcher.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tv_fetch/http_client.hpp"
#include "tv_fetch/support.hpp"

namespace tv_fetch::schedule {

namespace {

struct ScheduleEvent {
  std::time_t start = 0;
  std::time_t end = 0;
  std::string summary;
  std::string location;
  std::string description;
  bool all_day = false;
};

using ParamMap = std::map<std::string, std::string>;

AppError MakeScheduleError(std::string code,
                           std::string message,
                           std::string hint,
                           int exit_code = 6) {
  return AppError{
      .code = std::move(code),
      .message = std::move(message),
      .hint = std::move(hint),
      .exit_code = exit_code,
  };
}

std::time_t NowLocal() {
  return std::time(nullptr);
}

std::time_t ParseIsoLocal(std::string_view value) {
  if (value.empty()) {
    return NowLocal();
  }

  std::tm tm = {};
  try {
    if (value.size() < 16) {
      throw std::runtime_error("short");
    }
    tm.tm_year = std::stoi(std::string(value.substr(0, 4))) - 1900;
    tm.tm_mon = std::stoi(std::string(value.substr(5, 2))) - 1;
    tm.tm_mday = std::stoi(std::string(value.substr(8, 2)));
    tm.tm_hour = std::stoi(std::string(value.substr(11, 2)));
    tm.tm_min = std::stoi(std::string(value.substr(14, 2)));
    tm.tm_sec = 0;
    if (value.size() >= 19 && value[16] == ':') {
      tm.tm_sec = std::stoi(std::string(value.substr(17, 2)));
    }
  } catch (...) {
    throw MakeScheduleError("schedule_invalid_time",
                            "Now override must use ISO-8601 format.",
                            std::string(value));
  }
  tm.tm_isdst = -1;
  const std::time_t parsed = std::mktime(&tm);
  if (parsed == static_cast<std::time_t>(-1)) {
    throw MakeScheduleError("schedule_invalid_time",
                            "Failed to normalize the requested time.",
                            std::string(value));
  }
  return parsed;
}

std::tm LocalTm(std::time_t value) {
  const std::tm* local = std::localtime(&value);
  if (local == nullptr) {
    throw MakeScheduleError("schedule_time_failed",
                            "Failed to resolve local time.",
                            "std::localtime returned null.");
  }
  return *local;
}

std::string FormatKoreanDate(std::time_t value) {
  const std::tm tm = LocalTm(value);
  std::ostringstream stream;
  stream << (tm.tm_mon + 1) << "월 " << tm.tm_mday << "일";
  return stream.str();
}

std::string FormatKoreanDateTime(std::time_t value) {
  const std::tm tm = LocalTm(value);
  const bool is_am = tm.tm_hour < 12;
  int hour = tm.tm_hour % 12;
  if (hour == 0) {
    hour = 12;
  }
  std::ostringstream stream;
  stream << (is_am ? "오전 " : "오후 ") << hour << ":" << std::setfill('0')
         << std::setw(2) << tm.tm_min;
  return stream.str();
}

std::string ReadFile(std::string_view path) {
  std::ifstream handle{std::string(path)};
  if (!handle.is_open()) {
    throw MakeScheduleError("schedule_file_missing",
                            "Failed to open the requested ICS file.",
                            std::string(path));
  }
  std::ostringstream stream;
  stream << handle.rdbuf();
  return stream.str();
}

std::vector<std::string> UnfoldIcsLines(std::string_view text) {
  std::vector<std::string> unfolded;
  std::stringstream stream{std::string(text)};
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty() && (line.front() == ' ' || line.front() == '\t') &&
        !unfolded.empty()) {
      unfolded.back() += line.substr(1);
    } else {
      unfolded.push_back(line);
    }
  }
  return unfolded;
}

struct ContentLine {
  std::string name;
  ParamMap params;
  std::string value;
};

ContentLine SplitContentLine(std::string_view line) {
  const std::size_t colon = line.find(':');
  if (colon == std::string_view::npos) {
    return {};
  }
  const std::string_view head = line.substr(0, colon);
  const std::string_view value = line.substr(colon + 1);

  std::stringstream head_stream{std::string(head)};
  std::string segment;
  std::vector<std::string> parts;
  while (std::getline(head_stream, segment, ';')) {
    parts.push_back(segment);
  }

  ContentLine content;
  if (!parts.empty()) {
    content.name = parts.front();
  }
  for (std::size_t index = 1; index < parts.size(); ++index) {
    const std::size_t equals = parts[index].find('=');
    if (equals == std::string::npos) {
      continue;
    }
    content.params.emplace(parts[index].substr(0, equals),
                           parts[index].substr(equals + 1));
  }
  content.value = std::string(value);
  return content;
}

bool HasDateValue(const ParamMap& params) {
  const auto iter = params.find("VALUE");
  return iter != params.end() && iter->second == "DATE";
}

std::time_t ParseIcsTimestamp(std::string_view value,
                              const ParamMap& params,
                              bool* all_day) {
  *all_day = HasDateValue(params) || value.size() == 8;
  if (*all_day) {
    std::tm tm = {};
    tm.tm_year = std::stoi(std::string(value.substr(0, 4))) - 1900;
    tm.tm_mon = std::stoi(std::string(value.substr(4, 2))) - 1;
    tm.tm_mday = std::stoi(std::string(value.substr(6, 2)));
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    return std::mktime(&tm);
  }

  std::tm tm = {};
  tm.tm_year = std::stoi(std::string(value.substr(0, 4))) - 1900;
  tm.tm_mon = std::stoi(std::string(value.substr(4, 2))) - 1;
  tm.tm_mday = std::stoi(std::string(value.substr(6, 2)));
  tm.tm_hour = std::stoi(std::string(value.substr(9, 2)));
  tm.tm_min = std::stoi(std::string(value.substr(11, 2)));
  tm.tm_sec = value.size() >= 15 ? std::stoi(std::string(value.substr(13, 2))) : 0;
  tm.tm_isdst = -1;

  if (!value.empty() && value.back() == 'Z') {
    return timegm(&tm);
  }
  return std::mktime(&tm);
}

std::vector<ScheduleEvent> ParseIcsEvents(std::string_view text,
                                          std::time_t now,
                                          int window_days,
                                          int max_events) {
  const auto lines = UnfoldIcsLines(text);
  std::vector<std::vector<std::string>> blocks;
  std::vector<std::string>* current = nullptr;

  for (const auto& line : lines) {
    if (line == "BEGIN:VEVENT") {
      blocks.emplace_back();
      current = &blocks.back();
      continue;
    }
    if (line == "END:VEVENT") {
      current = nullptr;
      continue;
    }
    if (current != nullptr) {
      current->push_back(line);
    }
  }

  const std::time_t window_start = now - (4 * 60 * 60);
  const std::time_t window_end =
      now + static_cast<std::time_t>(std::max(1, window_days)) * 24 * 60 * 60;
  std::vector<ScheduleEvent> expanded;

  for (const auto& block : blocks) {
    std::map<std::string, std::vector<ContentLine>> props;
    for (const auto& line : block) {
      if (line.find(':') == std::string::npos) {
        continue;
      }
      ContentLine content = SplitContentLine(line);
      if (!content.name.empty()) {
        props[content.name].push_back(std::move(content));
      }
    }

    if (props.find("DTSTART") == props.end() || props.find("SUMMARY") == props.end()) {
      continue;
    }

    bool all_day = false;
    const auto& start_line = props["DTSTART"].front();
    const std::time_t start =
        ParseIcsTimestamp(start_line.value, start_line.params, &all_day);

    std::time_t end = start + (all_day ? 24 * 60 * 60 : 60 * 60);
    if (props.find("DTEND") != props.end()) {
      bool ignored_all_day = false;
      const auto& end_line = props["DTEND"].front();
      end = ParseIcsTimestamp(end_line.value, end_line.params, &ignored_all_day);
    }

    if (end < window_start || start > window_end) {
      continue;
    }

    expanded.push_back(ScheduleEvent{
        .start = start,
        .end = end,
        .summary = CleanText(props["SUMMARY"].front().value, 40),
        .location = CleanText(
            props.find("LOCATION") == props.end() ? "" : props["LOCATION"].front().value,
            32),
        .description = CleanText(
            props.find("DESCRIPTION") == props.end() ? ""
                                                     : props["DESCRIPTION"].front().value,
            80),
        .all_day = all_day,
    });
  }

  std::sort(expanded.begin(), expanded.end(), [](const ScheduleEvent& left,
                                                 const ScheduleEvent& right) {
    return left.start < right.start;
  });
  if (static_cast<int>(expanded.size()) > max_events) {
    expanded.resize(static_cast<std::size_t>(max_events));
  }
  return expanded;
}

std::string EventValue(const ScheduleEvent& event) {
  if (event.all_day) {
    return CleanText(event.summary + " · 종일", 40);
  }
  return CleanText(FormatKoreanDateTime(event.start) + " " + event.summary, 40);
}

std::string EventDetail(const ScheduleEvent& event) {
  std::vector<std::string> parts = {FormatKoreanDate(event.start)};
  if (event.all_day) {
    parts.push_back("종일");
  } else {
    parts.push_back(FormatKoreanDateTime(event.start) + "-" +
                    FormatKoreanDateTime(event.end));
  }
  if (!event.location.empty()) {
    parts.push_back(event.location);
  } else if (!event.description.empty()) {
    parts.push_back(event.description);
  }
  std::ostringstream stream;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (index > 0) {
      stream << " · ";
    }
    stream << parts[index];
  }
  return CleanText(stream.str(), 72);
}

JsonValue NormalizeIcs(std::string_view text, std::time_t now, int window_days,
                       int max_events) {
  const auto events = ParseIcsEvents(text, now, window_days, max_events);
  if (events.empty()) {
    return MakeObject({
        {"domain", JsonValue::String("schedule")},
        {"source", JsonValue::String("ics")},
        {"status", JsonValue::String("empty")},
        {"title", JsonValue::String("표시할 일정 없음")},
        {"headline", JsonValue::String("현재 창에서는 표시할 일정이 없습니다.")},
        {"primaryMetrics", JsonValue::Array()},
        {"sections", JsonValue::Array()},
        {"alert",
         MakeObject({
             {"icon", JsonValue::String("visibilityOff")},
             {"title", JsonValue::String("공용 화면 주의")},
             {"summary", JsonValue::String("개인 일정은 요약형으로만 표시하는 편이 안전합니다.")},
             {"meta", JsonValue::String("ICS 일정 요약")},
         })},
        {"actions",
         MakeArray({
             MakeObject({
                 {"label", JsonValue::String("새로고침")},
                 {"event", JsonValue::String("refreshSchedule")},
             }),
         })},
        {"footer", JsonValue::String("선택된 일정 창에 이벤트가 없으면 empty payload를 반환합니다.")},
    });
  }

  std::vector<ScheduleEvent> current_events;
  std::vector<ScheduleEvent> upcoming_events;
  for (const auto& event : events) {
    if (event.start <= now && now < event.end) {
      current_events.push_back(event);
    }
    if (event.end > now) {
      upcoming_events.push_back(event);
    }
  }
  if (upcoming_events.empty()) {
    upcoming_events = events;
  }

  const ScheduleEvent next_event = upcoming_events.front();
  const std::tm now_tm = LocalTm(now);
  std::tm today_end_tm = now_tm;
  today_end_tm.tm_hour = 23;
  today_end_tm.tm_min = 59;
  today_end_tm.tm_sec = 59;
  const std::time_t today_end = std::mktime(&today_end_tm);

  std::vector<ScheduleEvent> today_events;
  std::vector<ScheduleEvent> later_events;
  for (const auto& event : upcoming_events) {
    if (event.start <= today_end) {
      today_events.push_back(event);
    } else {
      later_events.push_back(event);
    }
  }

  JsonValue primary_metrics = JsonValue::Array();
  ArrayAppend(primary_metrics,
              MakeObject({
                  {"label", JsonValue::String("다음 일정")},
                  {"value", JsonValue::String(EventValue(next_event))},
                  {"detail", JsonValue::String(EventDetail(next_event))},
              }));
  ArrayAppend(primary_metrics,
              MakeObject({
                  {"label", JsonValue::String("오늘 남은 일정")},
                  {"value", JsonValue::String(std::to_string(today_events.size()) + "건")},
                  {"detail", JsonValue::String(FormatKoreanDate(now))},
              }));
  ArrayAppend(primary_metrics,
              MakeObject({
                  {"label", JsonValue::String("현재 진행")},
                  {"value",
                   JsonValue::String(current_events.empty() ? "없음"
                                                           : current_events.front().summary)},
                  {"detail",
                   JsonValue::String(current_events.empty()
                                         ? "현재 진행 중인 일정 없음"
                                         : EventDetail(current_events.front()))},
              }));

  JsonValue sections = JsonValue::Array();
  JsonValue now_items = JsonValue::Array();
  ArrayAppend(now_items,
              MakeObject({
                  {"icon", JsonValue::String("schedule")},
                  {"label", JsonValue::String("현재")},
                  {"value",
                   JsonValue::String(current_events.empty() ? "진행 중 일정 없음"
                                                           : current_events.front().summary)},
                  {"detail",
                   JsonValue::String(current_events.empty()
                                         ? "다음 일정만 준비합니다."
                                         : EventDetail(current_events.front()))},
              }));
  ArrayAppend(now_items,
              MakeObject({
                  {"icon", JsonValue::String("event")},
                  {"label", JsonValue::String("다음")},
                  {"value", JsonValue::String(next_event.summary)},
                  {"detail", JsonValue::String(EventDetail(next_event))},
              }));
  ArrayAppend(sections, MakeObject({
                            {"title", JsonValue::String("지금과 다음")},
                            {"items", std::move(now_items)},
                        }));

  if (!today_events.empty()) {
    JsonValue today_items = JsonValue::Array();
    for (std::size_t index = 0; index < today_events.size() && index < 4; ++index) {
      const auto& event = today_events[index];
      ArrayAppend(today_items,
                  MakeObject({
                      {"icon", JsonValue::String(event.all_day ? "today"
                                                               : "eventAvailable")},
                      {"label", JsonValue::String(event.summary)},
                      {"value", JsonValue::String(event.all_day ? "종일"
                                                                 : FormatKoreanDateTime(event.start))},
                      {"detail", JsonValue::String(EventDetail(event))},
                  }));
    }
    ArrayAppend(sections, MakeObject({
                              {"title", JsonValue::String("오늘 일정")},
                              {"items", std::move(today_items)},
                          }));
  }

  if (!later_events.empty()) {
    JsonValue later_items = JsonValue::Array();
    for (std::size_t index = 0; index < later_events.size() && index < 3; ++index) {
      const auto& event = later_events[index];
      ArrayAppend(later_items,
                  MakeObject({
                      {"icon", JsonValue::String("eventNote")},
                      {"label", JsonValue::String(event.summary)},
                      {"value", JsonValue::String(FormatKoreanDate(event.start))},
                      {"detail", JsonValue::String(EventDetail(event))},
                  }));
    }
    ArrayAppend(sections, MakeObject({
                              {"title", JsonValue::String("이후 일정")},
                              {"items", std::move(later_items)},
                          }));
  }

  return MakeObject({
      {"domain", JsonValue::String("schedule")},
      {"source", JsonValue::String("ics")},
      {"title", JsonValue::String("오늘 일정 브리핑")},
      {"headline",
       JsonValue::String(
           CleanText("ICS 일정 피드에서 현재와 다음 일정을 TV 거리에서 읽히는 형태로 정리했습니다.",
                     88))},
      {"primaryMetrics", std::move(primary_metrics)},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("visibilityOff")},
           {"title", JsonValue::String("공용 화면 주의")},
           {"summary",
            JsonValue::String(
                "참석자 이름, 상세 메모, 정확한 위치는 기본적으로 축약하거나 감춘 형태로 표시하는 편이 안전합니다.")},
           {"meta", JsonValue::String("ICS 일정 요약")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshSchedule")},
           }),
           MakeObject({
               {"label", JsonValue::String("내일 보기")},
               {"event", JsonValue::String("showTomorrowSchedule")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "현재 live adapter는 ICS URL 또는 파일을 지원합니다. 반복 일정은 일부 피드에서 기본 이벤트만 표시될 수 있습니다.")},
  });
}

}  // namespace

JsonResult LoadMockSchedulePayload() {
  return LoadFixturePayload("mock_schedule.json", "schedule", "mock");
}

JsonResult Execute(const ScheduleCommand& command) {
  if (command.source == ScheduleCommand::Source::kMock) {
    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("schedule")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           JsonValue::String(ResolveFixturePath("mock_schedule.json").string())},
      });
    }
    return LoadMockSchedulePayload();
  }

  try {
    if (command.dry_run) {
      JsonValue payload = JsonValue::Object();
      ObjectSet(payload, "command", JsonValue::String("schedule"));
      ObjectSet(payload, "mode", JsonValue::String("dry-run"));
      ObjectSet(payload, "source",
                JsonValue::String(command.source == ScheduleCommand::Source::kIcsUrl
                                      ? "ics-url"
                                      : "ics-file"));
      ObjectSet(payload, "days", JsonValue::Integer(command.days));
      ObjectSet(payload, "max_events", JsonValue::Integer(command.max_events));
      if (command.source == ScheduleCommand::Source::kIcsUrl) {
        ObjectSet(payload, "ics_url", JsonValue::String(command.ics_url));
      } else {
        ObjectSet(payload, "ics_file", JsonValue::String(command.ics_file));
      }
      return payload;
    }

    std::string text;
    if (command.source == ScheduleCommand::Source::kIcsUrl) {
      const auto response = HttpGet(command.ics_url);
      if (std::holds_alternative<AppError>(response)) {
        AppError error = std::get<AppError>(response);
        error.code = "schedule_fetch_failed";
        error.message = "ICS feed request failed.";
        error.hint = command.ics_url + " | " + error.hint;
        return error;
      }
      text = std::get<std::string>(response);
    } else {
      if (command.ics_file.empty()) {
        return MakeScheduleError("invalid_arguments",
                                 "ICS file source requires --ics-file.",
                                 "Use tv_fetch schedule --source ics-file --ics-file /path/file.ics.",
                                 2);
      }
      text = ReadFile(command.ics_file);
    }

    JsonValue payload = NormalizeIcs(text, ParseIsoLocal(command.now),
                                     command.days, command.max_events);
    ObjectSet(payload, "source",
              JsonValue::String(command.source == ScheduleCommand::Source::kIcsUrl
                                    ? "ics-url"
                                    : "ics-file"));
    return payload;
  } catch (const AppError& error) {
    return error;
  } catch (const std::exception& error) {
    return MakeScheduleError("schedule_parse_failed",
                             "Failed to normalize the ICS schedule feed.",
                             CleanText(error.what(), 120));
  }
}

}  // namespace tv_fetch::schedule
