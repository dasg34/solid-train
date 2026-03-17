#include "tv_fetch/commute/commute_fetcher.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "tv_fetch/http_client.hpp"
#include "tv_fetch/support.hpp"

namespace tv_fetch::commute {

namespace {

struct Place {
  double latitude = 0.0;
  double longitude = 0.0;
  std::string display_name;
};

struct LocalDateTime {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

AppError MakeCommuteError(std::string code,
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

LocalDateTime NowLocal() {
  const std::time_t now = std::time(nullptr);
  const std::tm* local = std::localtime(&now);
  if (local == nullptr) {
    throw MakeCommuteError("commute_time_failed",
                           "Failed to resolve local time.",
                           "std::localtime returned null.");
  }
  return LocalDateTime{
      .year = local->tm_year + 1900,
      .month = local->tm_mon + 1,
      .day = local->tm_mday,
      .hour = local->tm_hour,
      .minute = local->tm_min,
      .second = local->tm_sec,
  };
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
    throw MakeCommuteError("commute_time_failed",
                           "Failed to normalize date time.",
                           "std::mktime returned -1.");
  }
  return timestamp;
}

LocalDateTime FromTimeT(std::time_t timestamp) {
  const std::tm* local = std::localtime(&timestamp);
  if (local == nullptr) {
    throw MakeCommuteError("commute_time_failed",
                           "Failed to materialize date time.",
                           "std::localtime returned null.");
  }
  return LocalDateTime{
      .year = local->tm_year + 1900,
      .month = local->tm_mon + 1,
      .day = local->tm_mday,
      .hour = local->tm_hour,
      .minute = local->tm_min,
      .second = local->tm_sec,
  };
}

LocalDateTime ParseIsoLocal(std::string_view value,
                            const LocalDateTime& fallback) {
  if (value.empty()) {
    return fallback;
  }
  LocalDateTime parsed = {};
  if (value.size() < 16) {
    throw MakeCommuteError("commute_invalid_time",
                           "Time arguments must use ISO-8601 format.",
                           std::string(value));
  }
  try {
    parsed.year = std::stoi(std::string(value.substr(0, 4)));
    parsed.month = std::stoi(std::string(value.substr(5, 2)));
    parsed.day = std::stoi(std::string(value.substr(8, 2)));
    parsed.hour = std::stoi(std::string(value.substr(11, 2)));
    parsed.minute = std::stoi(std::string(value.substr(14, 2)));
    if (value.size() >= 19 && value[16] == ':') {
      parsed.second = std::stoi(std::string(value.substr(17, 2)));
    }
  } catch (...) {
    throw MakeCommuteError("commute_invalid_time",
                           "Time arguments must use ISO-8601 format.",
                           std::string(value));
  }
  return parsed;
}

LocalDateTime AddMinutes(const LocalDateTime& value, int minutes) {
  return FromTimeT(ToTimeT(value) + static_cast<std::time_t>(minutes) * 60);
}

int DifferenceMinutes(const LocalDateTime& left, const LocalDateTime& right) {
  const std::time_t left_ts = ToTimeT(left);
  const std::time_t right_ts = ToTimeT(right);
  return static_cast<int>(std::llround(
      std::difftime(left_ts, right_ts) / 60.0));
}

std::string FormatHhmm(const LocalDateTime& value) {
  std::ostringstream stream;
  stream << std::setfill('0') << std::setw(2) << value.hour << ":"
         << std::setw(2) << value.minute;
  return stream.str();
}

std::string FormatKoreanTime(const LocalDateTime& value) {
  const bool is_am = value.hour < 12;
  int hour = value.hour % 12;
  if (hour == 0) {
    hour = 12;
  }
  std::ostringstream stream;
  stream << (is_am ? "오전 " : "오후 ") << hour << ":"
         << std::setfill('0') << std::setw(2) << value.minute;
  return stream.str();
}

LocalDateTime DefaultArriveBy(const LocalDateTime& now) {
  LocalDateTime base = now;
  base.second = 0;
  int minutes_to_next_slot = (30 - (base.minute % 30)) % 30;
  LocalDateTime rounded = AddMinutes(base, minutes_to_next_slot);
  if (DifferenceMinutes(rounded, base) <= 20) {
    rounded = AddMinutes(rounded, 30);
  }
  return AddMinutes(rounded, 30);
}

std::string FormatDuration(double minutes) {
  const int total = std::max(0, static_cast<int>(std::lround(minutes)));
  const int hours = total / 60;
  const int remainder = total % 60;
  if (hours > 0 && remainder > 0) {
    return std::to_string(hours) + "시간 " + std::to_string(remainder) + "분";
  }
  if (hours > 0) {
    return std::to_string(hours) + "시간";
  }
  return std::to_string(remainder) + "분";
}

std::string FormatDistance(double meters) {
  std::ostringstream stream;
  if (meters >= 1000.0) {
    stream << std::fixed << std::setprecision(1) << (meters / 1000.0) << "km";
    return stream.str();
  }
  stream << std::lround(meters) << "m";
  return stream.str();
}

std::string NormalizeDisplayLabel(std::string_view raw_query,
                                  std::string_view display_name,
                                  std::string_view fallback) {
  const std::string raw = CleanText(raw_query, 18);
  if (!raw.empty() &&
      raw.find_first_of("0123456789") == std::string::npos &&
      raw.size() <= 18) {
    return raw;
  }

  std::string best;
  std::stringstream stream{std::string(display_name)};
  std::string token;
  int appended = 0;
  while (std::getline(stream, token, ',')) {
    const std::string cleaned = CleanText(token, 16);
    if (cleaned.empty() ||
        cleaned.find_first_of("0123456789") != std::string::npos) {
      continue;
    }
    if (!best.empty()) {
      best += ' ';
    }
    best += cleaned;
    ++appended;
    if (appended >= 2) {
      break;
    }
  }
  if (!best.empty()) {
    return CleanText(best, 18);
  }
  return CleanText(fallback, 18);
}

std::string BuildGeocodingUrl(std::string_view query) {
  return "https://nominatim.openstreetmap.org/search?q=" + UrlEncode(query) +
         "&format=jsonv2&limit=1&accept-language=ko-KR";
}

Place GeocodePlace(std::string_view query) {
  const std::string url = BuildGeocodingUrl(query);
  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "commute_geocode_failed";
    error.message = "Geocoding request failed.";
    error.hint = url + " | " + error.hint;
    throw error;
  }

  auto parsed = JsonValue::Parse(std::get<std::string>(response),
                                 "commute_parse_failed",
                                 "Geocoding response could not be parsed.", 6);
  if (std::holds_alternative<AppError>(parsed)) {
    throw std::get<AppError>(parsed);
  }

  const JsonValue results = std::get<JsonValue>(std::move(parsed));
  const JsonValue place = results.At(0);
  if (place.IsNull()) {
    throw MakeCommuteError("commute_no_match",
                           "Failed to resolve one of the requested places.",
                           std::string(query));
  }

  return Place{
      .latitude = std::stod(place.At("lat").AsString("0")),
      .longitude = std::stod(place.At("lon").AsString("0")),
      .display_name = CleanText(place.At("display_name").AsString(""), 80),
  };
}

std::string ProfileString(CommuteCommand::Profile profile) {
  return profile == CommuteCommand::Profile::kWalking ? "walking" : "driving";
}

std::string BuildRouteUrl(CommuteCommand::Profile profile,
                          const Place& origin,
                          const Place& destination) {
  std::ostringstream url;
  url << "https://router.project-osrm.org/route/v1/" << ProfileString(profile)
      << "/" << std::fixed << std::setprecision(6) << origin.longitude << ","
      << origin.latitude << ";" << destination.longitude << ","
      << destination.latitude
      << "?overview=false&steps=false&alternatives=true&annotations=false";
  return url.str();
}

JsonValue FetchRoutes(CommuteCommand::Profile profile,
                      const Place& origin,
                      const Place& destination) {
  const std::string url = BuildRouteUrl(profile, origin, destination);
  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "commute_route_failed";
    error.message = "Route request failed.";
    error.hint = url + " | " + error.hint;
    throw error;
  }
  auto parsed = JsonValue::Parse(std::get<std::string>(response),
                                 "commute_parse_failed",
                                 "Routing response could not be parsed.", 6);
  if (std::holds_alternative<AppError>(parsed)) {
    throw std::get<AppError>(parsed);
  }
  const JsonValue routes = std::get<JsonValue>(parsed).At("routes");
  if (!routes.IsArray() || routes.Size() == 0) {
    throw MakeCommuteError("commute_no_route",
                           "No OSRM route was available for the request.",
                           url);
  }
  return routes;
}

double RouteMinutes(const JsonValue& route) {
  return route.At("duration").AsDouble(0.0) / 60.0;
}

double RouteDistance(const JsonValue& route) {
  return route.At("distance").AsDouble(0.0);
}

std::pair<std::string, std::string> CommuteRisk(double primary_minutes,
                                                const std::optional<double>& alternate_minutes) {
  if (primary_minutes >= 75.0) {
    return {"높음", "장거리 이동"};
  }
  if (alternate_minutes.has_value()) {
    const double gap = std::fabs(*alternate_minutes - primary_minutes);
    if (gap >= 15.0) {
      return {"높음", "대안 경로 편차 큼"};
    }
    if (gap >= 7.0) {
      return {"보통", "대안 경로 차이 있음"};
    }
  }
  if (primary_minutes >= 45.0) {
    return {"보통", "도시권 혼잡 구간 가능"};
  }
  return {"낮음", "기본 경로 기준"};
}

std::string DepartureDetail(const LocalDateTime& now,
                            const LocalDateTime& leave_by) {
  const int delta = DifferenceMinutes(leave_by, now);
  if (delta <= 0) {
    if (std::abs(delta) <= 1) {
      return "지금 출발";
    }
    return std::to_string(std::abs(delta)) + "분 지연 상태";
  }
  if (delta >= 180) {
    return "약 " + FormatDuration(delta) + " 후";
  }
  return "지금부터 " + std::to_string(delta) + "분 후";
}

JsonValue BuildCommutePayload(const CommuteCommand& command) {
  const LocalDateTime now = ParseIsoLocal(command.now, NowLocal());
  const LocalDateTime target_arrival =
      ParseIsoLocal(command.arrive_by, DefaultArriveBy(now));

  const Place origin = GeocodePlace(command.origin);
  const Place destination = GeocodePlace(command.destination);
  const JsonValue routes = FetchRoutes(command.profile, origin, destination);
  const JsonValue primary = routes.At(0);
  const JsonValue alternate = routes.At(1);

  const double primary_minutes = RouteMinutes(primary);
  const double primary_distance = RouteDistance(primary);
  const std::optional<double> alternate_minutes =
      alternate.IsNull() ? std::nullopt : std::optional<double>(RouteMinutes(alternate));
  const std::optional<double> alternate_distance =
      alternate.IsNull() ? std::nullopt : std::optional<double>(RouteDistance(alternate));

  const LocalDateTime leave_by = AddMinutes(
      target_arrival,
      -static_cast<int>(std::lround(primary_minutes)) - std::max(0, command.buffer_minutes));

  const std::string origin_tv =
      command.origin_label.empty()
          ? NormalizeDisplayLabel(command.origin, origin.display_name, "출발지")
          : CleanText(command.origin_label, 18);
  const std::string destination_tv =
      command.destination_label.empty()
          ? NormalizeDisplayLabel(command.destination, destination.display_name, "목적지")
          : CleanText(command.destination_label, 18);

  const auto [risk_level, risk_detail] =
      CommuteRisk(primary_minutes, alternate_minutes);
  const std::string profile_label =
      command.profile == CommuteCommand::Profile::kWalking ? "도보" : "차량";

  JsonValue sections = JsonValue::Array();
  JsonValue primary_items = JsonValue::Array();
  ArrayAppend(
      primary_items,
      MakeObject({
          {"icon", JsonValue::String(command.profile == CommuteCommand::Profile::kWalking
                                         ? "directionsWalk"
                                         : "directionsCar")},
          {"label", JsonValue::String(profile_label)},
          {"value", JsonValue::String(FormatDuration(primary_minutes))},
          {"detail",
           JsonValue::String(CleanText(
               FormatDistance(primary_distance) + " · " + destination_tv + " 방향",
               60))},
      }));
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("주요 경로")},
          {"items", std::move(primary_items)},
      }));

  if (alternate_minutes.has_value() && alternate_distance.has_value()) {
    const int diff =
        static_cast<int>(std::lround(*alternate_minutes - primary_minutes));
    std::string diff_text = "기본 경로와 비슷함";
    if (diff > 0) {
      diff_text = "기본 경로보다 " + std::to_string(diff) + "분 더 김";
    } else if (diff < 0) {
      diff_text = "기본 경로보다 " + std::to_string(std::abs(diff)) + "분 더 짧음";
    }

    ArrayAppend(
        sections,
        MakeObject({
            {"title", JsonValue::String("대안")},
            {"items",
             MakeArray({
                 MakeObject({
                     {"icon", JsonValue::String("altRoute")},
                     {"label", JsonValue::String("대체 경로")},
                     {"value", JsonValue::String(FormatDuration(*alternate_minutes))},
                     {"detail",
                      JsonValue::String(CleanText(
                          FormatDistance(*alternate_distance) + " · " + diff_text,
                          60))},
                 }),
             })},
        }));
  }

  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("도착 목표")},
          {"items",
           MakeArray({
               MakeObject({
                   {"icon", JsonValue::String("event")},
                   {"label", JsonValue::String("도착 목표")},
                   {"value", JsonValue::String(FormatKoreanTime(target_arrival))},
                   {"detail",
                    JsonValue::String(CleanText(
                        FormatHhmm(leave_by) + " 출발 · 버퍼 " +
                            std::to_string(std::max(0, command.buffer_minutes)) + "분",
                        60))},
               }),
           })},
      }));

  return MakeObject({
      {"domain", JsonValue::String("commute")},
      {"source", JsonValue::String("osrm")},
      {"title", JsonValue::String("출근길 브리핑")},
      {"headline",
       JsonValue::String(CleanText(
           origin_tv + "에서 " + destination_tv + "까지 " +
               FormatDuration(primary_minutes) + " 예상, " +
               FormatHhmm(leave_by) + " 출발 권장입니다.",
           88))},
      {"primaryMetrics",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("추천 출발")},
               {"value", JsonValue::String(FormatHhmm(leave_by))},
               {"detail", JsonValue::String(DepartureDetail(now, leave_by))},
           }),
           MakeObject({
               {"label", JsonValue::String("예상 소요")},
               {"value", JsonValue::String(FormatDuration(primary_minutes))},
               {"detail", JsonValue::String(FormatDistance(primary_distance))},
           }),
           MakeObject({
               {"label", JsonValue::String("교통 리스크")},
               {"value", JsonValue::String(risk_level)},
               {"detail", JsonValue::String(CleanText(risk_detail, 28))},
           }),
       })},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("traffic")},
           {"title", JsonValue::String("실시간 교통 미반영")},
           {"summary",
            JsonValue::String(
                "현재 live adapter는 Nominatim 지오코딩과 OSRM 기본 경로를 사용합니다. 실시간 교통 정체나 대중교통 지연은 별도 피드가 필요합니다.")},
           {"meta", JsonValue::String("OSRM · OpenStreetMap")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshCommute")},
           }),
           MakeObject({
               {"label", JsonValue::String("대안 보기")},
               {"event", JsonValue::String("showAlternateRoute")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "공용 TV에서는 출발지와 목적지의 정확한 주소 대신 요약된 위치 표현을 유지하는 편이 안전합니다.")},
  });
}

}  // namespace

JsonResult LoadMockCommutePayload() {
  return LoadFixturePayload("mock_commute.json", "commute", "mock");
}

JsonResult Execute(const CommuteCommand& command) {
  if (command.source == CommuteCommand::Source::kMock) {
    if (command.dry_run) {
      const auto path = ResolveFixturePath("mock_commute.json");
      return MakeObject({
          {"command", JsonValue::String("commute")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           path.empty() ? JsonValue::Null() : JsonValue::String(path.string())},
      });
    }
    return LoadMockCommutePayload();
  }

  if (command.dry_run) {
    return MakeObject({
        {"command", JsonValue::String("commute")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("osrm")},
        {"request",
         MakeObject({
             {"origin", JsonValue::String(command.origin)},
             {"destination", JsonValue::String(command.destination)},
             {"origin_geocode_url",
              JsonValue::String(BuildGeocodingUrl(command.origin))},
             {"destination_geocode_url",
              JsonValue::String(BuildGeocodingUrl(command.destination))},
             {"route_template",
              JsonValue::String(
                  "https://router.project-osrm.org/route/v1/{profile}/{origin_lon},{origin_lat};{destination_lon},{destination_lat}?overview=false&steps=false&alternatives=true&annotations=false")},
             {"profile", JsonValue::String(ProfileString(command.profile))},
             {"buffer_minutes", JsonValue::Integer(command.buffer_minutes)},
         })},
    });
  }

  try {
    return BuildCommutePayload(command);
  } catch (const AppError& error) {
    return error;
  }
}

}  // namespace tv_fetch::commute
