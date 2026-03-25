#include "tizen_tool_domain_fetch/emergency/emergency_fetcher.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::emergency {

namespace {

constexpr std::string_view kSpecialReportUrl =
    "https://www.weather.go.kr/w/special-report/list.do";
constexpr std::string_view kEarthquakeUrl =
    "https://www.weather.go.kr/w/earthquake-volcano/search/korea.do";

struct SpecialReportOption {
  std::string value;
  std::string kind;
  std::string label;
};

struct EarthquakeEvent {
  std::time_t occurred_at = 0;
  double magnitude = 0.0;
  int depth_km = 0;
  std::string max_intensity;
  std::string location;
};

AppError MakeEmergencyError(std::string code,
                            std::string message,
                            std::string hint,
                            int exit_code = 6) {
  return AppError{std::move(code), std::move(message), std::move(hint),
                  exit_code};
}

std::time_t ParseIsoLocal(std::string_view value) {
  if (value.empty()) {
    return std::time(nullptr);
  }
  std::tm tm = {};
  try {
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
    throw MakeEmergencyError("emergency_invalid_time",
                             "Now override must use ISO-8601 format.",
                             std::string(value));
  }
  tm.tm_isdst = -1;
  const std::time_t parsed = std::mktime(&tm);
  if (parsed == static_cast<std::time_t>(-1)) {
    throw MakeEmergencyError("emergency_invalid_time",
                             "Failed to normalize the requested time.",
                             std::string(value));
  }
  return parsed;
}

std::tm LocalTm(std::time_t value) {
  const std::tm* local = std::localtime(&value);
  if (local == nullptr) {
    throw MakeEmergencyError("emergency_time_failed",
                             "Failed to resolve local time.",
                             "std::localtime returned null.");
  }
  return *local;
}

std::string FormatHhmm(std::time_t value) {
  const std::tm tm = LocalTm(value);
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(2) << tm.tm_hour << ":"
         << std::setw(2) << tm.tm_min;
  return stream.str();
}

std::string FormatRelativeDays(std::time_t now, std::time_t target) {
  const int delta_seconds = static_cast<int>(std::difftime(now, target));
  if (delta_seconds < 3600) {
    return std::to_string(std::max(0, delta_seconds / 60)) + "분 전";
  }
  if (delta_seconds < 24 * 3600) {
    return std::to_string(std::max(0, delta_seconds / 3600)) + "시간 전";
  }
  return std::to_string(std::max(0, delta_seconds / (24 * 3600))) + "일 전";
}

std::string RegexFirst(std::string_view text, const std::regex& pattern) {
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return "";
  }
  return std::string(match[1].first, match[1].second);
}

std::string StripTags(std::string value) {
  value = std::regex_replace(
      value, std::regex("<br\\s*/?>", std::regex::icase), "\n");
  value = std::regex_replace(
      value,
      std::regex("</(p|li|div|h4|strong|figcaption|em)>", std::regex::icase),
      "\n");
  value = std::regex_replace(value, std::regex("<[^>]+>"), " ");

  const std::vector<std::pair<std::string, std::string>> replacements = {
      {"&nbsp;", " "},
      {"&amp;", "&"},
      {"&lt;", "<"},
      {"&gt;", ">"},
      {"&quot;", "\""},
      {"&#39;", "'"},
  };
  for (const auto& [from, to] : replacements) {
    std::size_t cursor = 0;
    while ((cursor = value.find(from, cursor)) != std::string::npos) {
      value.replace(cursor, from.size(), to);
      cursor += to.size();
    }
  }
  return value;
}

std::vector<std::string> SplitByPipe(std::string_view value) {
  std::stringstream stream{std::string(value)};
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(stream, part, '|')) {
    const std::string cleaned = CleanText(part, 140);
    if (!cleaned.empty()) {
      parts.push_back(cleaned);
    }
  }
  return parts;
}

std::vector<std::string> SplitLines(std::string value, int max_items) {
  value = StripTags(std::move(value));
  value = std::regex_replace(value, std::regex("○"), "\n○ ");
  value = std::regex_replace(value, std::regex("□"), "\n□ ");
  std::stringstream stream(value);
  std::string line;
  std::vector<std::string> parts;
  while (std::getline(stream, line)) {
    std::string cleaned = CleanText(line, 120);
    cleaned = std::regex_replace(cleaned, std::regex(R"(^[○□\-]\s*)"), "");
    const std::size_t status_pos = cleaned.find("(현황)");
    if (status_pos != std::string::npos) {
      cleaned = cleaned.substr(status_pos);
    }
    const std::size_t note_pos = cleaned.find("(전망)");
    if (note_pos != std::string::npos) {
      cleaned = cleaned.substr(note_pos);
    }
    if (cleaned.empty() || cleaned == "□ 내용" || cleaned.front() == '<') {
      continue;
    }
    if (cleaned == "내용" || cleaned == "(현황)" || cleaned == "(전망)") {
      continue;
    }
    if (cleaned.find("내용") != std::string::npos && cleaned.size() <= 16) {
      continue;
    }
    parts.push_back(cleaned);
    if (static_cast<int>(parts.size()) >= max_items) {
      break;
    }
  }
  return parts;
}

std::time_t ParseKmaDateTime(std::string_view value) {
  static const std::regex pattern(
      R"((\d{4})년\s*(\d{2})월\s*(\d{2})일\s*(\d{2})시\s*(\d{2})분)");
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(value.begin(), value.end(), match, pattern) ||
      match.size() < 6) {
    return 0;
  }
  std::tm tm = {};
  tm.tm_year = std::stoi(std::string(match[1].first, match[1].second)) - 1900;
  tm.tm_mon = std::stoi(std::string(match[2].first, match[2].second)) - 1;
  tm.tm_mday = std::stoi(std::string(match[3].first, match[3].second));
  tm.tm_hour = std::stoi(std::string(match[4].first, match[4].second));
  tm.tm_min = std::stoi(std::string(match[5].first, match[5].second));
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return std::mktime(&tm);
}

std::string EmergencyLevelLabel(std::string_view kind) {
  if (kind == "met") {
    return "경보";
  }
  if (kind == "pwn") {
    return "주의";
  }
  if (kind == "ann") {
    return "속보";
  }
  return "안내";
}

std::string EmergencyTypeLabel(std::string_view kind) {
  if (kind == "met") {
    return "특보";
  }
  if (kind == "pwn") {
    return "예비특보";
  }
  if (kind == "ann") {
    return "속보";
  }
  return "기상정보";
}

int EmergencyPriority(std::string_view kind) {
  if (kind == "met") {
    return 0;
  }
  if (kind == "pwn") {
    return 1;
  }
  if (kind == "ann") {
    return 2;
  }
  return 3;
}

std::vector<SpecialReportOption> FetchSpecialReportOptions() {
  const auto response = HttpGet(kSpecialReportUrl);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "emergency_special_report_failed";
    error.message = "KMA special report list request failed.";
    error.hint = std::string(kSpecialReportUrl) + " | " + error.hint;
    throw error;
  }

  const std::string page = std::get<std::string>(response);
  const std::string select_block = RegexFirst(
      page, std::regex(R"(<select id="select-list" name="reportId">([\s\S]*?)</select>)"));
  if (select_block.empty()) {
    return {};
  }

  std::vector<SpecialReportOption> options;
  const std::regex option_pattern(
      R"__REGEX__(<option value="([^"]*)"[^>]*>([\s\S]*?)</option>)__REGEX__");
  for (std::sregex_iterator iter(select_block.begin(), select_block.end(), option_pattern),
       end;
       iter != end; ++iter) {
    const std::string value = CleanText((*iter)[1].str(), 80);
    if (value.find(':') == std::string::npos) {
      continue;
    }
    const std::string label = CleanText(StripTags((*iter)[2].str()), 88);
    options.push_back(
        SpecialReportOption{value, value.substr(0, value.find(':')), label});
  }
  return options;
}

std::optional<SpecialReportOption> SelectSpecialReport(
    std::vector<SpecialReportOption> options) {
  if (options.empty()) {
    return std::nullopt;
  }
  std::sort(options.begin(), options.end(),
            [](const SpecialReportOption& left,
               const SpecialReportOption& right) {
              return EmergencyPriority(left.kind) < EmergencyPriority(right.kind);
            });
  return options.front();
}

JsonValue BuildSpecialReportPayload(const SpecialReportOption& report,
                                    std::time_t now) {
  const std::string detail_url =
      std::string(kSpecialReportUrl) + "?kind=" + UrlEncode(report.kind) +
      "&reportId=" + UrlEncode(report.value);
  const auto response = HttpGet(detail_url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "emergency_special_report_failed";
    error.message = "KMA special report detail request failed.";
    error.hint = detail_url + " | " + error.hint;
    throw error;
  }

  const std::string page = std::get<std::string>(response);
  const std::string announce_html = RegexFirst(
      page, std::regex(R"(<div class="cmp-view-announce">([\s\S]*?)</div>)"));
  const std::string header_html = RegexFirst(
      page, std::regex(R"(<div class="cmp-view-header">\s*<h4>([\s\S]*?)</h4>)"));
  const std::string content_html = RegexFirst(
      page, std::regex(R"(<div class="cmp-view-content">([\s\S]*?)</section>)"));
  if (announce_html.empty() || header_html.empty() || content_html.empty()) {
    throw MakeEmergencyError("emergency_special_report_invalid",
                             "Failed to parse the KMA special report page.",
                             detail_url);
  }

  const auto announce_parts = SplitByPipe(StripTags(announce_html));
  const auto header_parts = SplitByPipe(StripTags(header_html));
  const std::vector<std::string> summary_lines = SplitLines(content_html, 3);

  const std::string area =
      announce_parts.empty() ? "전국" : CleanText(announce_parts[0], 28);
  const std::string published_text =
      announce_parts.size() >= 2 ? announce_parts[1] : "";
  const std::time_t published_at = ParseKmaDateTime(published_text);
  const std::string announce_meta =
      announce_parts.size() >= 3 ? announce_parts[2] : "";

  const std::string report_number =
      header_parts.empty() ? report.value : CleanText(header_parts[0], 24);
  const std::string report_type = header_parts.size() >= 2
                                      ? CleanText(header_parts[1], 24)
                                      : EmergencyTypeLabel(report.kind);
  const std::string subject = header_parts.size() >= 3
                                  ? CleanText(header_parts[2], 88)
                                  : CleanText(report.label, 88);
  const std::string summary = summary_lines.empty()
                                  ? subject
                                  : CleanText(summary_lines[0] +
                                                  (summary_lines.size() >= 2
                                                       ? " " + summary_lines[1]
                                                       : ""),
                                              100);

  JsonValue sections = JsonValue::Array();
  JsonValue primary_items = JsonValue::Array();
  ArrayAppend(primary_items,
              MakeObject({
                  {"icon",
                   JsonValue::String(report.kind == "inf" ? "info" : "warning")},
                  {"label", JsonValue::String(report_type)},
                  {"value",
                   JsonValue::String(
                       CleanText(summary_lines.empty() ? subject : summary_lines[0], 32))},
                  {"detail",
                   JsonValue::String(
                       CleanText(summary_lines.size() >= 2 ? summary_lines[1] : summary, 72))},
              }));
  ArrayAppend(sections, MakeObject({
                            {"title", JsonValue::String("핵심 안내")},
                            {"items", std::move(primary_items)},
                        }));

  JsonValue official_items = JsonValue::Array();
  ArrayAppend(official_items,
              MakeObject({
                  {"icon", JsonValue::String("place")},
                  {"label", JsonValue::String("영향 지역")},
                  {"value", JsonValue::String(area)},
                  {"detail",
                   JsonValue::String(
                       CleanText(announce_meta.empty() ? published_text : announce_meta,
                                 72))},
              }));
  ArrayAppend(sections, MakeObject({
                            {"title", JsonValue::String("공식 확인")},
                            {"items", std::move(official_items)},
                        }));

  JsonValue metrics = JsonValue::Array();
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("알림 수준")},
                  {"value", JsonValue::String(EmergencyLevelLabel(report.kind))},
                  {"detail", JsonValue::String(report_type)},
              }));
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("영향 지역")},
                  {"value", JsonValue::String(area)},
                  {"detail", JsonValue::String(report_number)},
              }));
  ArrayAppend(metrics,
              MakeObject({
                  {"label", JsonValue::String("발표 시각")},
                  {"value", JsonValue::String(published_at == 0 ? "-" : FormatHhmm(published_at))},
                  {"detail",
                   JsonValue::String(
                       published_at == 0 ? "발표 시각 확인" : FormatRelativeDays(now, published_at))},
              }));

  return MakeObject({
      {"domain", JsonValue::String("emergency")},
      {"source", JsonValue::String("kma-special-report")},
      {"title",
       JsonValue::String(report.kind == "inf" ? "공식 주의 정보" : "긴급 알림 모드")},
      {"headline", JsonValue::String(subject)},
      {"primaryMetrics", std::move(metrics)},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("report")},
           {"title", JsonValue::String(report_type)},
           {"summary", JsonValue::String(summary)},
           {"meta", JsonValue::String("기상청 날씨누리")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshEmergencyAlert")},
           }),
           MakeObject({
               {"label", JsonValue::String("상세 확인")},
               {"event", JsonValue::String("openEmergencyDetail")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "공식 특보와 기상정보를 기준으로 TV용 요약만 제공합니다. 실제 행동 판단은 기상청 원문과 재난문자를 우선하세요.")},
  });
}

std::vector<EarthquakeEvent> ParseEarthquakeEvents(std::string_view page) {
  const std::string tbody = RegexFirst(
      page,
      std::regex(
          R"(<table class="table-col eqk-search-table whitespaced" id="excel_body">[\s\S]*?<tbody>([\s\S]*?)</tbody>)"));
  if (tbody.empty()) {
    return {};
  }

  std::vector<EarthquakeEvent> events;
  const std::regex row_pattern(R"(<tr>([\s\S]*?)</tr>)");
  const std::regex cell_pattern(R"(<td[^>]*>([\s\S]*?)</td>)");
  for (std::sregex_iterator row_iter(tbody.begin(), tbody.end(), row_pattern), end;
       row_iter != end; ++row_iter) {
    std::vector<std::string> cells;
    const std::string row_html = (*row_iter)[1].str();
    for (std::sregex_iterator cell_iter(row_html.begin(), row_html.end(), cell_pattern);
         cell_iter != end; ++cell_iter) {
      cells.push_back(CleanText(StripTags((*cell_iter)[1].str()), 120));
    }
    if (cells.size() < 10) {
      continue;
    }

    std::tm tm = {};
    std::stringstream time_stream(cells[1]);
    time_stream >> std::get_time(&tm, "%Y/%m/%d %H:%M:%S");
    if (time_stream.fail()) {
      continue;
    }
    tm.tm_isdst = -1;
    const std::time_t occurred_at = std::mktime(&tm);

    try {
      events.push_back(EarthquakeEvent{
          occurred_at,
          std::stod(cells[2]),
          static_cast<int>(std::lround(std::stod(cells[3]))),
          CleanText(cells[4], 12),
          CleanText(cells[7], 40),
      });
    } catch (...) {
      continue;
    }
  }
  return events;
}

std::string EarthquakeLevel(const EarthquakeEvent& event) {
  if (event.magnitude >= 4.5) {
    return "경계";
  }
  if (event.magnitude >= 3.5) {
    return "주의";
  }
  return "관찰";
}

std::optional<JsonValue> FetchEarthquakePayload(const EmergencyCommand& command,
                                                std::time_t now) {
  const auto response = HttpGet(kEarthquakeUrl);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "emergency_earthquake_failed";
    error.message = "KMA earthquake list request failed.";
    error.hint = std::string(kEarthquakeUrl) + " | " + error.hint;
    throw error;
  }

  std::vector<EarthquakeEvent> events =
      ParseEarthquakeEvents(std::get<std::string>(response));
  const std::time_t max_age =
      static_cast<std::time_t>(std::max(1, command.max_age_days)) * 24 * 60 * 60;
  events.erase(std::remove_if(events.begin(), events.end(),
                              [&](const EarthquakeEvent& event) {
                                return event.magnitude < command.min_magnitude ||
                                       std::difftime(now, event.occurred_at) > max_age;
                              }),
               events.end());
  if (events.empty()) {
    return std::nullopt;
  }

  const EarthquakeEvent& event = events.front();
  return MakeObject({
      {"domain", JsonValue::String("emergency")},
      {"source", JsonValue::String("kma-earthquake")},
      {"title", JsonValue::String("긴급 알림 모드")},
      {"headline",
       JsonValue::String(
           CleanText(event.location + " 규모 " + std::to_string(event.magnitude) +
                         " 지진이 관측되었습니다.",
                     88))},
      {"primaryMetrics",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("지진 규모")},
               {"value", JsonValue::String(CleanText(std::to_string(event.magnitude), 8))},
               {"detail", JsonValue::String(EarthquakeLevel(event))},
           }),
           MakeObject({
               {"label", JsonValue::String("최대 진도")},
               {"value", JsonValue::String(event.max_intensity)},
               {"detail", JsonValue::String(CleanText(event.location, 28))},
           }),
           MakeObject({
               {"label", JsonValue::String("발생 시각")},
               {"value", JsonValue::String(FormatHhmm(event.occurred_at))},
               {"detail", JsonValue::String(FormatRelativeDays(now, event.occurred_at))},
           }),
       })},
      {"sections",
       MakeArray({
           MakeObject({
               {"title", JsonValue::String("발생 정보")},
               {"items",
                MakeArray({
                    MakeObject({
                        {"icon", JsonValue::String("warning")},
                        {"label", JsonValue::String("발생 위치")},
                        {"value", JsonValue::String(CleanText(event.location, 32))},
                        {"detail",
                         JsonValue::String(CleanText(
                             "최대진도 " + event.max_intensity + " · 깊이 " +
                                 std::to_string(event.depth_km) + "km",
                             72))},
                    }),
                    MakeObject({
                        {"icon", JsonValue::String("schedule")},
                        {"label", JsonValue::String("공식 발표")},
                        {"value", JsonValue::String(FormatHhmm(event.occurred_at))},
                        {"detail", JsonValue::String("기상청 국내지진조회 기준")},
                    }),
                })},
           }),
           MakeObject({
               {"title", JsonValue::String("즉시 확인")},
               {"items",
                MakeArray({
                    MakeObject({
                        {"icon", JsonValue::String("warning")},
                        {"label", JsonValue::String("추가 안내")},
                        {"value", JsonValue::String("실내 낙하물 주의")},
                        {"detail", JsonValue::String("여진 가능성과 시설물 상태를 공식 안내로 다시 확인하세요.")},
                    }),
                })},
           }),
       })},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("report")},
           {"title", JsonValue::String("최근 유의 지진")},
           {"summary",
            JsonValue::String(CleanText(
                event.location + "에서 규모 " + std::to_string(event.magnitude) +
                    ", 최대진도 " + event.max_intensity + "가 기록되었습니다.",
                100))},
           {"meta", JsonValue::String("기상청 지진·화산")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshEmergencyAlert")},
           }),
           MakeObject({
               {"label", JsonValue::String("상세 확인")},
               {"event", JsonValue::String("openEmergencyDetail")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "지진 정보는 기상청 공개 목록을 기준으로 요약했습니다. 대피 판단은 재난문자와 공식 행동요령을 우선하세요.")},
  });
}

JsonValue EmptyEmergencyPayload(std::string summary) {
  return MakeObject({
      {"domain", JsonValue::String("emergency")},
      {"source", JsonValue::String("live")},
      {"status", JsonValue::String("empty")},
      {"title", JsonValue::String("현재 경보 없음")},
      {"headline", JsonValue::String("표시할 공식 경보가 없습니다.")},
      {"primaryMetrics", JsonValue::Array()},
      {"sections", JsonValue::Array()},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("info")},
           {"title", JsonValue::String("현재 경보 없음")},
           {"summary", JsonValue::String(CleanText(summary, 100))},
           {"meta", JsonValue::String("기상청 공식 소스")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshEmergencyAlert")},
           }),
       })},
      {"footer", JsonValue::String("무경보 상태도 명시적으로 유지해 TV 화면에서 혼동을 줄입니다.")},
  });
}

}  // namespace

JsonResult LoadMockEmergencyPayload() {
  return LoadFixturePayload("mock_emergency.json", "emergency", "mock");
}

JsonResult Execute(const EmergencyCommand& command) {
  if (command.source == EmergencyCommand::Source::kMock) {
    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("emergency")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           JsonValue::String(ResolveFixturePath("mock_emergency.json").string())},
      });
    }
    return LoadMockEmergencyPayload();
  }

  try {
    const std::time_t now = ParseIsoLocal(command.now);
    if (command.dry_run) {
      std::string source = "kma-combined";
      if (command.source == EmergencyCommand::Source::kKmaSpecialReport) {
        source = "kma-special-report";
      } else if (command.source == EmergencyCommand::Source::kKmaEarthquake) {
        source = "kma-earthquake";
      }
      return MakeObject({
          {"command", JsonValue::String("emergency")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String(source)},
          {"special_report_url", JsonValue::String(kSpecialReportUrl)},
          {"earthquake_url", JsonValue::String(kEarthquakeUrl)},
      });
    }

    if (command.source == EmergencyCommand::Source::kKmaSpecialReport) {
      const auto report = SelectSpecialReport(FetchSpecialReportOptions());
      if (!report.has_value()) {
        return EmptyEmergencyPayload("현재 표시할 공식 특보나 기상정보가 없습니다.");
      }
      return BuildSpecialReportPayload(*report, now);
    }

    if (command.source == EmergencyCommand::Source::kKmaEarthquake) {
      const auto payload = FetchEarthquakePayload(command, now);
      if (!payload.has_value()) {
        return EmptyEmergencyPayload("최근 유의 지진이 없습니다.");
      }
      return *payload;
    }

    const auto report = SelectSpecialReport(FetchSpecialReportOptions());
    if (report.has_value()) {
      return BuildSpecialReportPayload(*report, now);
    }
    const auto payload = FetchEarthquakePayload(command, now);
    if (payload.has_value()) {
      return *payload;
    }
    return EmptyEmergencyPayload("현재 공식 특보와 최근 유의 지진이 없습니다.");
  } catch (const AppError& error) {
    return error;
  } catch (const std::exception& error) {
    return MakeEmergencyError("emergency_live_failed",
                              "Failed to build the live emergency payload.",
                              CleanText(error.what(), 120));
  }
}

}  // namespace tizen_tool_domain_fetch::emergency
