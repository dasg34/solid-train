#include "tizen_tool_domain_fetch/daily/daily_fetcher.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "tizen_tool_domain_fetch/commute/commute_fetcher.hpp"
#include "tizen_tool_domain_fetch/news/news_fetcher.hpp"
#include "tizen_tool_domain_fetch/schedule/schedule_fetcher.hpp"
#include "tizen_tool_domain_fetch/support.hpp"
#include "tizen_tool_domain_fetch/weather/weather_fetcher.hpp"

namespace tizen_tool_domain_fetch::daily {

namespace {

AppError MakeDailyError(std::string code,
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

std::string JsonString(const JsonValue& object, std::string_view key,
                       std::string_view fallback = {}) {
  return object.At(key).AsString(fallback);
}

std::string ShortHourLabel(std::string_view value) {
  const std::string text = CleanText(value, 32);
  if (text.size() >= 16 && text[10] == 'T') {
    return text.substr(11, 5);
  }
  return text;
}

JsonValue FirstMetric(const JsonValue& payload, std::size_t index) {
  const JsonValue metrics = payload.At("primaryMetrics");
  if (!metrics.IsArray() || metrics.Size() <= index) {
    return JsonValue::Null();
  }
  return metrics.At(index);
}

JsonValue MetricMatching(const JsonValue& payload, std::string_view keyword) {
  const JsonValue metrics = payload.At("primaryMetrics");
  if (!metrics.IsArray()) {
    return JsonValue::Null();
  }
  for (std::size_t index = 0; index < metrics.Size(); ++index) {
    const JsonValue metric = metrics.At(index);
    if (CleanText(JsonString(metric, "label"), 24).find(keyword) !=
        std::string::npos) {
      return metric;
    }
  }
  return JsonValue::Null();
}

std::vector<JsonValue> FirstItems(const JsonValue& payload, std::size_t max_items) {
  std::vector<JsonValue> items;
  const JsonValue sections = payload.At("sections");
  if (!sections.IsArray()) {
    return items;
  }
  for (std::size_t section_index = 0; section_index < sections.Size(); ++section_index) {
    const JsonValue section = sections.At(section_index);
    const JsonValue section_items = section.At("items");
    if (!section_items.IsArray()) {
      continue;
    }
    for (std::size_t item_index = 0; item_index < section_items.Size(); ++item_index) {
      items.push_back(section_items.At(item_index));
      if (items.size() >= max_items) {
        return items;
      }
    }
  }
  return items;
}

std::optional<std::string> ExtractScheduleArriveBy(const JsonValue& payload) {
  JsonValue metric = MetricMatching(payload, "다음");
  if (metric.IsNull()) {
    metric = FirstMetric(payload, 0);
  }
  std::string text = CleanText(JsonString(metric, "value"), 40);
  if (text.empty()) {
    return std::nullopt;
  }

  const std::time_t now = std::time(nullptr);
  const std::tm* local = std::localtime(&now);
  if (local == nullptr) {
    return std::nullopt;
  }
  std::tm target = *local;
  target.tm_sec = 0;

  auto build_target = [&](int hour, int minute) -> std::optional<std::string> {
    target.tm_hour = hour;
    target.tm_min = minute;
    std::time_t target_ts = std::mktime(&target);
    if (target_ts == static_cast<std::time_t>(-1)) {
      return std::nullopt;
    }
    if (target_ts <= now) {
      target.tm_mday += 1;
      target_ts = std::mktime(&target);
      if (target_ts == static_cast<std::time_t>(-1)) {
        return std::nullopt;
      }
    }
    const std::tm* adjusted = std::localtime(&target_ts);
    if (adjusted == nullptr) {
      return std::nullopt;
    }
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(4) << adjusted->tm_year + 1900 << "-"
           << std::setw(2) << adjusted->tm_mon + 1 << "-" << std::setw(2)
           << adjusted->tm_mday << "T" << std::setw(2) << adjusted->tm_hour
           << ":" << std::setw(2) << adjusted->tm_min << ":" << std::setw(2)
           << adjusted->tm_sec;
    return stream.str();
  };

  const std::vector<std::string> prefixes = {"오전 ", "오후 "};
  for (const auto& prefix : prefixes) {
    if (text.rfind(prefix, 0) == 0) {
      const std::string token = text.substr(prefix.size());
      const std::size_t colon = token.find(':');
      if (colon == std::string::npos) {
        return std::nullopt;
      }
      try {
        int hour = std::stoi(token.substr(0, colon)) % 12;
        const int minute = std::stoi(token.substr(colon + 1, 2));
        if (prefix == "오후 ") {
          hour += 12;
        }
        return build_target(hour, minute);
      } catch (...) {
        return std::nullopt;
      }
    }
  }

  const std::size_t colon = text.find(':');
  if (colon != std::string::npos) {
    try {
      const int hour = std::stoi(text.substr(0, colon));
      const int minute = std::stoi(text.substr(colon + 1, 2));
      return build_target(hour, minute);
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

struct CardBundle {
  JsonValue section;
  JsonValue metric;
  std::string snippet;
};

CardBundle ComposeWeatherSection(const JsonValue& payload) {
  const JsonValue location = payload.At("location");
  const JsonValue current = payload.At("current");
  const std::string city = CleanText(JsonString(location, "city", "서울"), 12);
  const std::string district = CleanText(JsonString(location, "district"), 12);
  const std::string condition = CleanText(JsonString(current, "condition", "날씨 정보"), 18);
  const std::string temperature =
      CleanText(std::to_string(current.At("temperature_c").AsInt(0)) + "°", 8);
  const std::string feels_like =
      CleanText(std::to_string(current.At("feels_like_c").AsInt(0)) + "°", 8);
  const std::string precip =
      CleanText(std::to_string(current.At("precip_probability_pct").AsInt(0)) + "%", 8);

  JsonValue items = JsonValue::Array();
  ArrayAppend(items,
              MakeObject({
                  {"icon", JsonValue::String("wbSunny")},
                  {"label", JsonValue::String("현재")},
                  {"value", JsonValue::String(CleanText(condition + " " + temperature, 28))},
                  {"detail",
                   JsonValue::String(CleanText(
                       (district.empty() ? city : district) + " · 체감 " + feels_like +
                           " · 강수확률 " + precip,
                       72))},
              }));
  const JsonValue hourly = payload.At("hourly");
  if (hourly.IsArray()) {
    for (std::size_t index = 0; index < hourly.Size() && index < 2; ++index) {
      const JsonValue hour = hourly.At(index);
      ArrayAppend(items,
                  MakeObject({
                      {"icon", JsonValue::String("schedule")},
                      {"label",
                       JsonValue::String(CleanText(ShortHourLabel(JsonString(hour, "time", "예보")), 12))},
                      {"value",
                       JsonValue::String(CleanText(
                           JsonString(hour, "condition", "날씨") + " " +
                               CleanText(std::to_string(hour.At("temperature_c").AsInt(0)) + "°",
                                         8),
                           28))},
                      {"detail",
                       JsonValue::String(CleanText(
                           "강수확률 " +
                               std::to_string(hour.At("precip_probability_pct").AsInt(0)) + "%",
                           64))},
                  }));
    }
  }

  return CardBundle{
      .section = MakeObject({
          {"title", JsonValue::String("오늘 날씨")},
          {"items", std::move(items)},
      }),
      .metric = MakeObject({
          {"label", JsonValue::String("현재 날씨")},
          {"value", JsonValue::String(CleanText(condition + " " + temperature, 28))},
          {"detail", JsonValue::String(CleanText("체감 " + feels_like, 32))},
      }),
      .snippet = CleanText(JsonString(payload, "headline", city + " 현재 " + condition), 36),
  };
}

CardBundle ComposeNewsSection(const JsonValue& payload) {
  JsonValue metric_source = FirstMetric(payload, 1);
  if (metric_source.IsNull()) {
    metric_source = FirstMetric(payload, 0);
  }
  JsonValue items = JsonValue::Array();
  const auto item_values = FirstItems(payload, 3);
  for (const auto& item : item_values) {
    ArrayAppend(items,
                MakeObject({
                    {"icon", JsonValue::String(CleanText(JsonString(item, "icon", "article"), 20))},
                    {"label", JsonValue::String(CleanText(JsonString(item, "label", "최신"), 20))},
                    {"value", JsonValue::String(CleanText(JsonString(item, "value"), 30))},
                    {"detail", JsonValue::String(CleanText(JsonString(item, "detail"), 72))},
                }));
  }
  const std::string lead =
      item_values.empty() ? "주요 뉴스를 준비 중입니다."
                          : CleanText(JsonString(item_values.front(), "value"), 36);
  return CardBundle{
      .section = MakeObject({
          {"title", JsonValue::String("주요 뉴스")},
          {"items", std::move(items)},
      }),
      .metric = MakeObject({
          {"label", JsonValue::String(CleanText(JsonString(metric_source, "label", "헤드라인"), 20))},
          {"value", JsonValue::String(CleanText(JsonString(metric_source, "value", "0건"), 20))},
          {"detail", JsonValue::String(CleanText(JsonString(metric_source, "detail", "뉴스 카드"), 32))},
      }),
      .snippet = lead,
  };
}

CardBundle ComposeScheduleSection(const JsonValue& payload) {
  JsonValue metric_source = MetricMatching(payload, "다음");
  if (metric_source.IsNull()) {
    metric_source = FirstMetric(payload, 0);
  }
  JsonValue items = JsonValue::Array();
  for (const auto& item : FirstItems(payload, 3)) {
    ArrayAppend(items,
                MakeObject({
                    {"icon", JsonValue::String(CleanText(JsonString(item, "icon", "schedule"), 20))},
                    {"label", JsonValue::String(CleanText(JsonString(item, "label"), 18))},
                    {"value", JsonValue::String(CleanText(JsonString(item, "value"), 30))},
                    {"detail", JsonValue::String(CleanText(JsonString(item, "detail"), 72))},
                }));
  }
  const std::string label =
      CleanText(JsonString(metric_source, "label", "다음 일정"), 20);
  const std::string value =
      CleanText(JsonString(metric_source, "value", "일정 확인 필요"), 28);
  return CardBundle{
      .section = MakeObject({
          {"title", JsonValue::String("다음 일정")},
          {"items", std::move(items)},
      }),
      .metric = MakeObject({
          {"label", JsonValue::String(label)},
          {"value", JsonValue::String(value)},
          {"detail", JsonValue::String(CleanText(JsonString(metric_source, "detail",
                                                             "캘린더 연결 상태 확인"),
                                                 32))},
      }),
      .snippet = CleanText(label + " " + value, 36),
  };
}

CardBundle ComposeCommuteSection(const JsonValue& payload) {
  JsonValue metric_source = MetricMatching(payload, "추천");
  if (metric_source.IsNull()) {
    metric_source = FirstMetric(payload, 0);
  }
  JsonValue items = JsonValue::Array();
  for (const auto& item : FirstItems(payload, 3)) {
    ArrayAppend(items,
                MakeObject({
                    {"icon", JsonValue::String(CleanText(JsonString(item, "icon", "traffic"), 20))},
                    {"label", JsonValue::String(CleanText(JsonString(item, "label"), 18))},
                    {"value", JsonValue::String(CleanText(JsonString(item, "value"), 30))},
                    {"detail", JsonValue::String(CleanText(JsonString(item, "detail"), 72))},
                }));
  }
  const std::string label =
      CleanText(JsonString(metric_source, "label", "추천 출발"), 20);
  const std::string value =
      CleanText(JsonString(metric_source, "value", "경로 확인 필요"), 28);
  return CardBundle{
      .section = MakeObject({
          {"title", JsonValue::String("출근")},
          {"items", std::move(items)},
      }),
      .metric = MakeObject({
          {"label", JsonValue::String(label)},
          {"value", JsonValue::String(value)},
          {"detail",
           JsonValue::String(CleanText(JsonString(metric_source, "detail", "이동 카드"), 32))},
      }),
      .snippet = CleanText(JsonString(payload, "headline", label + " " + value), 36),
  };
}

JsonValue ComposeAlert(const std::optional<JsonValue>& weather_payload,
                       const std::vector<std::string>& missing_cards,
                       const std::vector<std::string>& partial_reasons,
                       const std::vector<std::string>& sources_used) {
  if (!missing_cards.empty()) {
    std::ostringstream summary;
    for (std::size_t index = 0; index < partial_reasons.size(); ++index) {
      if (index > 0) {
        summary << " / ";
      }
      summary << partial_reasons[index];
    }
    std::ostringstream meta;
    for (std::size_t index = 0; index < missing_cards.size(); ++index) {
      if (index > 0) {
        meta << ", ";
      }
      meta << missing_cards[index];
    }
    return MakeObject({
        {"icon", JsonValue::String("dashboard")},
        {"title", JsonValue::String("일부 카드만 표시 중")},
        {"summary",
         JsonValue::String(CleanText(summary.str().empty()
                                         ? "일부 카드를 준비하지 못해 표시 가능한 카드만 먼저 보여줍니다."
                                         : summary.str(),
                                     100))},
        {"meta", JsonValue::String(CleanText(meta.str(), 40))},
    });
  }

  if (weather_payload.has_value()) {
    const JsonValue weather_alert = weather_payload->At("alert");
    const std::string title = CleanText(JsonString(weather_alert, "title"), 28);
    const std::string summary = CleanText(JsonString(weather_alert, "summary"), 100);
    if (!title.empty() || !summary.empty()) {
      return MakeObject({
          {"icon", JsonValue::String("warning")},
          {"title", JsonValue::String(title.empty() ? "날씨 알림" : title)},
          {"summary", JsonValue::String(summary)},
          {"meta", JsonValue::String(CleanText(JsonString(weather_alert, "source"), 40))},
      });
    }
  }

  std::ostringstream meta;
  for (std::size_t index = 0; index < sources_used.size(); ++index) {
    if (index > 0) {
      meta << " · ";
    }
    meta << sources_used[index];
  }
  return MakeObject({
      {"icon", JsonValue::String("info")},
      {"title", JsonValue::String("출처와 시각 유지")},
      {"summary",
       JsonValue::String(
           "조합형 브리핑은 각 카드의 출처와 갱신 시각을 따로 유지하는 편이 신뢰 판단에 유리합니다.")},
      {"meta", JsonValue::String(CleanText(meta.str(), 48))},
  });
}

std::string ComposeFooter(const std::vector<std::string>& sources_used,
                          const std::vector<std::string>& partial_reasons) {
  std::string base =
      "기본 순서는 날씨, 일정, 출근, 뉴스이며 각 카드는 TV 거리에서 빠르게 읽히는 길이로 축약합니다.";
  if (!sources_used.empty()) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < sources_used.size(); ++index) {
      if (index > 0) {
        stream << ", ";
      }
      stream << sources_used[index];
    }
    base += " 사용 소스: " + stream.str() + ".";
  }
  if (!partial_reasons.empty()) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < partial_reasons.size(); ++index) {
      if (index > 0) {
        stream << " / ";
      }
      stream << partial_reasons[index];
    }
    base += " 일부 카드 상태: " + stream.str() + ".";
  }
  return CleanText(base, 100);
}

std::optional<JsonValue> ExtractPayload(JsonResult result) {
  if (std::holds_alternative<AppError>(result)) {
    throw std::get<AppError>(result);
  }
  return std::get<JsonValue>(std::move(result));
}

JsonValue ComposeLivePayload(const DailyCommand& command) {
  std::vector<std::string> partial_reasons;
  std::vector<std::string> missing_cards;
  std::vector<std::string> sources_used;

  std::optional<JsonValue> weather_payload;
  std::optional<JsonValue> news_payload;
  std::optional<JsonValue> schedule_payload;
  std::optional<JsonValue> commute_payload;
  std::optional<CardBundle> weather_card;
  std::optional<CardBundle> news_card;
  std::optional<CardBundle> schedule_card;
  std::optional<CardBundle> commute_card;

  try {
    if (command.weather_source != DailyCommand::WeatherSource::kSkip) {
      WeatherCommand weather_command;
      weather_command.source =
          command.weather_source == DailyCommand::WeatherSource::kMock
              ? WeatherCommand::Source::kMock
              : WeatherCommand::Source::kOpenMeteo;
      weather_command.city = command.city;
      weather_command.district = command.district;
      weather_command.latitude = command.latitude;
      weather_command.longitude = command.longitude;
      weather_command.hours = command.weather_hours;
      weather_payload = ExtractPayload(weather::Execute(weather_command));
      weather_card = ComposeWeatherSection(*weather_payload);
      sources_used.push_back(command.weather_source == DailyCommand::WeatherSource::kMock
                                 ? "날씨 mock"
                                 : "날씨 Open-Meteo");
    }
  } catch (const std::exception& error) {
    missing_cards.push_back("날씨");
    partial_reasons.push_back("날씨 연결 실패: " + CleanText(error.what(), 44));
  }

  try {
    if (command.news_source != DailyCommand::NewsSource::kSkip) {
      NewsCommand news_command;
      news_command.source =
          command.news_source == DailyCommand::NewsSource::kMock
              ? NewsCommand::Source::kMock
              : NewsCommand::Source::kYonhapRss;
      news_command.rss_url = command.rss_url;
      news_command.count = command.news_count;
      news_payload = ExtractPayload(news::Execute(news_command));
      news_card = ComposeNewsSection(*news_payload);
      sources_used.push_back(command.news_source == DailyCommand::NewsSource::kMock
                                 ? "뉴스 mock"
                                 : "뉴스 연합뉴스TV RSS");
    }
  } catch (const std::exception& error) {
    missing_cards.push_back("뉴스");
    partial_reasons.push_back("뉴스 연결 실패: " + CleanText(error.what(), 44));
  }

  try {
    if (command.schedule_source != DailyCommand::ScheduleSource::kSkip) {
      ScheduleCommand schedule_command;
      if (command.schedule_source == DailyCommand::ScheduleSource::kMock) {
        schedule_command.source = ScheduleCommand::Source::kMock;
      } else if (command.schedule_source == DailyCommand::ScheduleSource::kIcsFile) {
        schedule_command.source = ScheduleCommand::Source::kIcsFile;
      } else {
        schedule_command.source = ScheduleCommand::Source::kIcsUrl;
      }
      schedule_command.ics_url = command.ics_url;
      schedule_command.ics_file = command.ics_file;
      schedule_command.days = command.schedule_days;
      schedule_command.max_events = command.schedule_max_events;
      schedule_command.now = command.schedule_now;

      schedule_payload = ExtractPayload(schedule::Execute(schedule_command));
      if (JsonString(*schedule_payload, "status") == "empty") {
        missing_cards.push_back("일정");
        partial_reasons.push_back("현재 창에서는 일정 카드가 비어 있습니다.");
      } else {
        schedule_card = ComposeScheduleSection(*schedule_payload);
      }
      sources_used.push_back(command.schedule_source == DailyCommand::ScheduleSource::kMock
                                 ? "일정 mock"
                                 : (command.schedule_source ==
                                            DailyCommand::ScheduleSource::kIcsFile
                                        ? "일정 ICS file"
                                        : "일정 ICS URL"));
    }
  } catch (const std::exception& error) {
    missing_cards.push_back("일정");
    partial_reasons.push_back("일정 연결 실패: " + CleanText(error.what(), 44));
  }

  try {
    if (command.commute_source != DailyCommand::CommuteSource::kSkip) {
      CommuteCommand commute_command;
      commute_command.source =
          command.commute_source == DailyCommand::CommuteSource::kMock
              ? CommuteCommand::Source::kMock
              : CommuteCommand::Source::kOsrm;
      commute_command.origin = command.commute_origin;
      commute_command.destination = command.commute_destination;
      commute_command.origin_label = command.commute_origin_label;
      commute_command.destination_label = command.commute_destination_label;
      commute_command.profile = command.commute_profile;
      commute_command.now = command.commute_now;
      commute_command.buffer_minutes = command.commute_buffer_minutes;
      if (!command.commute_arrive_by.empty()) {
        commute_command.arrive_by = command.commute_arrive_by;
      } else if (schedule_payload.has_value()) {
        const auto extracted = ExtractScheduleArriveBy(*schedule_payload);
        if (extracted.has_value()) {
          commute_command.arrive_by = *extracted;
        }
      }
      commute_payload = ExtractPayload(commute::Execute(commute_command));
      commute_card = ComposeCommuteSection(*commute_payload);
      sources_used.push_back(command.commute_source == DailyCommand::CommuteSource::kMock
                                 ? "출근 mock"
                                 : "출근 Nominatim + OSRM");
    }
  } catch (const std::exception& error) {
    missing_cards.push_back("출근");
    partial_reasons.push_back("출근 연결 실패: " + CleanText(error.what(), 44));
  }

  JsonValue sections = JsonValue::Array();
  JsonValue metrics = JsonValue::Array();
  std::vector<std::string> snippets;

  for (const auto* card : {weather_card ? &*weather_card : nullptr,
                           schedule_card ? &*schedule_card : nullptr,
                           commute_card ? &*commute_card : nullptr,
                           news_card ? &*news_card : nullptr}) {
    if (card == nullptr) {
      continue;
    }
    ArrayAppend(sections, card->section);
    ArrayAppend(metrics, card->metric);
    snippets.push_back(card->snippet);
  }

  if (sections.Size() == 0) {
    throw MakeDailyError("daily_no_cards",
                         "No live daily cards were available.",
                         "Check the selected sources or retry with mock fallbacks.");
  }

  JsonValue actions = JsonValue::Array();
  ArrayAppend(actions,
              MakeObject({
                  {"label", JsonValue::String("새로고침")},
                  {"event", JsonValue::String("refreshDailyBriefing")},
              }));
  if (weather_card.has_value()) {
    ArrayAppend(actions, MakeObject({
                              {"label", JsonValue::String("날씨")},
                              {"event", JsonValue::String("openWeatherCard")},
                          }));
  }
  if (schedule_card.has_value() && actions.Size() < 3) {
    ArrayAppend(actions, MakeObject({
                              {"label", JsonValue::String("일정")},
                              {"event", JsonValue::String("openScheduleCard")},
                          }));
  }
  if (commute_card.has_value() && actions.Size() < 3) {
    ArrayAppend(actions, MakeObject({
                              {"label", JsonValue::String("출근")},
                              {"event", JsonValue::String("openCommuteCard")},
                          }));
  }
  if (news_card.has_value() && actions.Size() < 3) {
    ArrayAppend(actions, MakeObject({
                              {"label", JsonValue::String("뉴스")},
                              {"event", JsonValue::String("openNewsCard")},
                          }));
  }

  std::ostringstream headline;
  for (std::size_t index = 0; index < snippets.size(); ++index) {
    if (index > 0) {
      headline << " · ";
    }
    headline << snippets[index];
  }

  return MakeObject({
      {"domain", JsonValue::String("daily")},
      {"source", JsonValue::String("compose-live")},
      {"title", JsonValue::String("오늘의 브리핑")},
      {"headline",
       JsonValue::String(CleanText(
           headline.str().empty() ? "오늘 아침에 필요한 핵심 카드만 추렸습니다."
                                  : headline.str(),
           88))},
      {"primaryMetrics", metrics},
      {"sections", sections},
      {"alert", ComposeAlert(weather_payload, missing_cards, partial_reasons, sources_used)},
      {"actions", actions},
      {"footer", JsonValue::String(ComposeFooter(sources_used, partial_reasons))},
  });
}

}  // namespace

JsonResult LoadMockDailyPayload() {
  return LoadFixturePayload("mock_daily.json", "daily", "mock");
}

JsonResult Execute(const DailyCommand& command) {
  if (command.source == DailyCommand::Source::kMock) {
    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("daily")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           JsonValue::String(ResolveFixturePath("mock_daily.json").string())},
      });
    }
    return LoadMockDailyPayload();
  }

  if (command.dry_run) {
    return MakeObject({
        {"command", JsonValue::String("daily")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("compose-live")},
        {"weather_source",
         JsonValue::String(command.weather_source == DailyCommand::WeatherSource::kMock
                               ? "mock"
                               : (command.weather_source ==
                                          DailyCommand::WeatherSource::kSkip
                                      ? "skip"
                                      : "open-meteo"))},
        {"news_source",
         JsonValue::String(command.news_source == DailyCommand::NewsSource::kMock
                               ? "mock"
                               : (command.news_source == DailyCommand::NewsSource::kSkip
                                      ? "skip"
                                      : "yonhap-rss"))},
        {"schedule_source",
         JsonValue::String(command.schedule_source ==
                                   DailyCommand::ScheduleSource::kMock
                               ? "mock"
                               : (command.schedule_source ==
                                          DailyCommand::ScheduleSource::kIcsFile
                                      ? "ics-file"
                                      : (command.schedule_source ==
                                                 DailyCommand::ScheduleSource::kSkip
                                             ? "skip"
                                             : "ics-url")))},
        {"commute_source",
         JsonValue::String(command.commute_source == DailyCommand::CommuteSource::kMock
                               ? "mock"
                               : (command.commute_source ==
                                          DailyCommand::CommuteSource::kSkip
                                      ? "skip"
                                      : "osrm"))},
    });
  }

  try {
    return ComposeLivePayload(command);
  } catch (const AppError& error) {
    return error;
  } catch (const std::exception& error) {
    return MakeDailyError("daily_live_failed",
                          "Failed to build the live daily payload.",
                          CleanText(error.what(), 120));
  }
}

}  // namespace tizen_tool_domain_fetch::daily
