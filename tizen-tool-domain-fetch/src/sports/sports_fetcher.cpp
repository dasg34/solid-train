#include "tizen_tool_domain_fetch/sports/sports_fetcher.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::sports {

namespace {

struct LeaguePreset {
  std::string id;
  std::string name;
};

AppError MakeSportsError(std::string code,
                         std::string message,
                         std::string hint,
                         int exit_code = 6) {
  return AppError{std::move(code), std::move(message), std::move(hint),
                  exit_code};
}

LeaguePreset ResolveLeaguePreset(const SportsCommand& command) {
  LeaguePreset preset;
  if (command.league == "kleague1") {
    preset = LeaguePreset{"4689", "K League 1"};
  } else if (command.league == "kleague2") {
    preset = LeaguePreset{"4822", "K League 2"};
  } else if (command.league == "kbo") {
    preset = LeaguePreset{"4830", "KBO League"};
  } else {
    throw MakeSportsError("sports_invalid_league",
                          "Unsupported sports league preset.",
                          command.league);
  }

  if (!command.league_id.empty()) {
    preset.id = command.league_id;
  }
  if (!command.league_name.empty()) {
    preset.name = command.league_name;
  }
  return preset;
}

JsonValue FetchJson(std::string_view url) {
  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "sports_request_failed";
    error.message = "Sports data request failed.";
    error.hint = std::string(url) + " | " + error.hint;
    throw error;
  }
  auto parsed = JsonValue::Parse(std::get<std::string>(response),
                                 "sports_parse_failed",
                                 "Sports response could not be parsed.", 6);
  if (std::holds_alternative<AppError>(parsed)) {
    throw std::get<AppError>(parsed);
  }
  return std::get<JsonValue>(std::move(parsed));
}

std::string EventSortKey(const JsonValue& event) {
  const std::string date = event.At("dateEventLocal").AsString(
      event.At("dateEvent").AsString("9999-99-99"));
  const std::string time = event.At("strTimeLocal").AsString(
      event.At("strTime").AsString("99:99:99"));
  return date + "T" + time;
}

bool IsFinished(const JsonValue& event) {
  std::string status = event.At("strStatus").AsString("");
  std::transform(status.begin(), status.end(), status.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (status == "ft" || status == "match finished" || status == "aet" ||
      status == "pen") {
    return true;
  }
  return !event.At("intHomeScore").IsNull() && !event.At("intAwayScore").IsNull();
}

std::string EventLabel(const JsonValue& event) {
  return CleanText(event.At("strHomeTeam").AsString("홈") + " vs " +
                       event.At("strAwayTeam").AsString("원정"),
                   40);
}

std::string EventValue(const JsonValue& event) {
  const std::string home = CleanText(event.At("strHomeTeam").AsString("홈"), 18);
  const std::string away = CleanText(event.At("strAwayTeam").AsString("원정"), 18);
  if (!event.At("intHomeScore").IsNull() && !event.At("intAwayScore").IsNull()) {
    return CleanText(home + " " + event.At("intHomeScore").AsString("0") + " : " +
                         event.At("intAwayScore").AsString("0") + " " + away,
                     44);
  }
  const std::string date = event.At("dateEventLocal").AsString(
      event.At("dateEvent").AsString(""));
  const std::string time = event.At("strTimeLocal").AsString(
      event.At("strTime").AsString(""));
  if (date.empty()) {
    return CleanText(home + " vs " + away, 44);
  }
  return CleanText(home + " vs " + away + " " + date + " " + time, 44);
}

std::string EventDetail(const JsonValue& event) {
  const std::string date = event.At("dateEventLocal").AsString(
      event.At("dateEvent").AsString(""));
  const std::string time = event.At("strTimeLocal").AsString(
      event.At("strTime").AsString(""));
  const std::string status = CleanText(event.At("strStatus").AsString("상태 확인 필요"), 20);
  const std::string venue = CleanText(event.At("strVenue").AsString(""), 28);
  std::string detail = date.empty() ? status : CleanText(date + " " + time + " · " + status, 60);
  if (!venue.empty()) {
    detail = CleanText(detail + " · " + venue, 60);
  }
  return detail;
}

std::string MainIconForLeague(std::string_view league_name) {
  return league_name.find("KBO") != std::string_view::npos ? "sportsBaseball"
                                                           : "sportsSoccer";
}

JsonValue NormalizeSportsPayload(const LeaguePreset& preset) {
  const std::string next_url =
      "https://www.thesportsdb.com/api/v1/json/123/eventsnextleague.php?id=" +
      preset.id;
  const std::string past_url =
      "https://www.thesportsdb.com/api/v1/json/123/eventspastleague.php?id=" +
      preset.id;

  JsonValue next_events = FetchJson(next_url).At("events");
  JsonValue past_events = FetchJson(past_url).At("events");

  std::vector<JsonValue> all_events;
  std::unordered_set<std::string> seen_ids;
  auto append_events = [&all_events, &seen_ids](const JsonValue& events) {
    if (!events.IsArray()) {
      return;
    }
    for (std::size_t index = 0; index < events.Size(); ++index) {
      JsonValue event = events.At(index);
      const std::string event_id = event.At("idEvent").AsString("");
      if (!event_id.empty() && seen_ids.find(event_id) != seen_ids.end()) {
        continue;
      }
      if (!event_id.empty()) {
        seen_ids.insert(event_id);
      }
      all_events.push_back(event);
    }
  };
  append_events(past_events);
  append_events(next_events);

  std::vector<JsonValue> recent;
  std::vector<JsonValue> upcoming;
  for (const auto& event : all_events) {
    if (IsFinished(event)) {
      recent.push_back(event);
    } else {
      upcoming.push_back(event);
    }
  }

  std::sort(recent.begin(), recent.end(), [](const JsonValue& left, const JsonValue& right) {
    return EventSortKey(left) > EventSortKey(right);
  });
  std::sort(upcoming.begin(), upcoming.end(),
            [](const JsonValue& left, const JsonValue& right) {
              return EventSortKey(left) < EventSortKey(right);
            });

  if (recent.empty() && upcoming.empty()) {
    throw MakeSportsError("sports_empty_feed",
                          "No sports events were available for the selected league.",
                          preset.id);
  }

  const JsonValue hero = recent.empty() ? upcoming.front() : recent.front();
  JsonValue primary_metrics = JsonValue::Array();
  ArrayAppend(
      primary_metrics,
      MakeObject({
          {"label", JsonValue::String("리그")},
          {"value", JsonValue::String(preset.name)},
          {"detail", JsonValue::String("실시간 피드")},
      }));
  if (!recent.empty()) {
    ArrayAppend(
        primary_metrics,
        MakeObject({
            {"label", JsonValue::String("최근 결과")},
            {"value", JsonValue::String(EventValue(recent.front()))},
            {"detail", JsonValue::String(EventDetail(recent.front()))},
        }));
  }
  if (!upcoming.empty()) {
    ArrayAppend(
        primary_metrics,
        MakeObject({
            {"label", JsonValue::String("다음 경기")},
            {"value", JsonValue::String(EventLabel(upcoming.front()))},
            {"detail", JsonValue::String(EventDetail(upcoming.front()))},
        }));
  }

  JsonValue sections = JsonValue::Array();
  if (!recent.empty()) {
    JsonValue recent_items = JsonValue::Array();
    for (std::size_t index = 0; index < recent.size() && index < 3; ++index) {
      ArrayAppend(
          recent_items,
          MakeObject({
              {"icon", JsonValue::String(MainIconForLeague(preset.name))},
              {"label", JsonValue::String(EventLabel(recent[index]))},
              {"value", JsonValue::String(EventValue(recent[index]))},
              {"detail", JsonValue::String(EventDetail(recent[index]))},
          }));
    }
    ArrayAppend(
        sections,
        MakeObject({
            {"title", JsonValue::String("최근 경기")},
            {"items", std::move(recent_items)},
        }));
  }

  if (!upcoming.empty()) {
    JsonValue upcoming_items = JsonValue::Array();
    for (std::size_t index = 0; index < upcoming.size() && index < 3; ++index) {
      ArrayAppend(
          upcoming_items,
          MakeObject({
              {"icon", JsonValue::String("schedule")},
              {"label", JsonValue::String(EventLabel(upcoming[index]))},
              {"value", JsonValue::String(EventValue(upcoming[index]))},
              {"detail", JsonValue::String(EventDetail(upcoming[index]))},
          }));
    }
    ArrayAppend(
        sections,
        MakeObject({
            {"title", JsonValue::String("예정 경기")},
            {"items", std::move(upcoming_items)},
        }));
  }

  return MakeObject({
      {"domain", JsonValue::String("sports")},
      {"source", JsonValue::String("thesportsdb")},
      {"title", JsonValue::String("오늘의 스포츠 브리핑")},
      {"headline",
       JsonValue::String(CleanText(
           preset.name + " 기준 최신 경기와 다음 일정을 정리했습니다.", 88))},
      {"primaryMetrics", std::move(primary_metrics)},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("update")},
           {"title", JsonValue::String("갱신 시각 표시 필요")},
           {"summary",
            JsonValue::String(
                "실시간 경기로 보이는 순간 stale data에 민감해지므로 마지막 갱신 시각과 리그 출처를 함께 보여주는 편이 안전합니다.")},
           {"meta", JsonValue::String("TheSportsDB")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshSports")},
           }),
           MakeObject({
               {"label", JsonValue::String("리그 변경")},
               {"event", JsonValue::String("changeLeague")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "기본 live preset은 한국 리그 중심이며, 더 정확한 라이브 스코어가 필요하면 전용 유료 피드 또는 공식 제휴 소스를 검토하는 편이 좋습니다.")},
  });
}

}  // namespace

JsonResult LoadMockSportsPayload() {
  return LoadFixturePayload("mock_sports.json", "sports", "mock");
}

JsonResult Execute(const SportsCommand& command) {
  if (command.source == SportsCommand::Source::kMock) {
    if (command.dry_run) {
      const auto path = ResolveFixturePath("mock_sports.json");
      return MakeObject({
          {"command", JsonValue::String("sports")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           path.empty() ? JsonValue::Null() : JsonValue::String(path.string())},
      });
    }
    return LoadMockSportsPayload();
  }

  try {
    const LeaguePreset preset = ResolveLeaguePreset(command);
    if (command.dry_run) {
      return MakeObject({
          {"command", JsonValue::String("sports")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("thesportsdb")},
          {"request",
           MakeObject({
               {"league", JsonValue::String(command.league)},
               {"league_id", JsonValue::String(preset.id)},
               {"league_name", JsonValue::String(preset.name)},
               {"next_url",
                JsonValue::String(
                    "https://www.thesportsdb.com/api/v1/json/123/eventsnextleague.php?id=" +
                    preset.id)},
               {"past_url",
                JsonValue::String(
                    "https://www.thesportsdb.com/api/v1/json/123/eventspastleague.php?id=" +
                    preset.id)},
           })},
      });
    }
    return NormalizeSportsPayload(preset);
  } catch (const AppError& error) {
    return error;
  }
}

}  // namespace tizen_tool_domain_fetch::sports
