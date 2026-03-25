#include "tizen_tool_domain_fetch/finance/finance_fetcher.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iconv.h>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::finance {

namespace {

constexpr std::string_view kNaverRealtimeBase =
    "https://polling.finance.naver.com/api/realtime?query=";
constexpr std::string_view kFrankfurterBase =
    "https://api.frankfurter.dev/v1";

struct WatchlistEntry {
  std::string code;
  std::string label;
};

struct MarketSnapshot {
  std::string code;
  std::string label;
  double current = 0.0;
  double change = 0.0;
  double change_pct = 0.0;
  std::string status;
};

struct ExchangeSnapshot {
  std::string date;
  double usd_krw = 0.0;
  double usd_krw_change = 0.0;
  double jpy100_krw = 0.0;
  double jpy100_krw_change = 0.0;
};

AppError MakeFinanceError(std::string code,
                          std::string message,
                          std::string hint,
                          int exit_code = 6) {
  return AppError{std::move(code), std::move(message), std::move(hint),
                  exit_code};
}

std::vector<WatchlistEntry> ParseWatchlist(std::string_view raw_value) {
  std::vector<WatchlistEntry> entries;
  std::string token;
  std::stringstream stream{std::string(raw_value)};
  while (std::getline(stream, token, ',')) {
    const std::size_t begin = token.find_first_not_of(" \t");
    if (begin == std::string::npos) {
      continue;
    }
    const std::size_t end = token.find_last_not_of(" \t");
    token = token.substr(begin, end - begin + 1);
    if (token.empty()) {
      continue;
    }

    std::string code = token;
    std::string label = token;
    const std::size_t colon = token.find(':');
    if (colon != std::string::npos) {
      code = token.substr(0, colon);
      label = token.substr(colon + 1);
    }

    if (code.size() != 6 ||
        !std::all_of(code.begin(), code.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
      throw MakeFinanceError(
          "finance_invalid_watchlist",
          "Watchlist entries must use 6-digit domestic stock codes.",
          std::string(raw_value));
    }

    entries.push_back(WatchlistEntry{code, CleanText(label, 24)});
  }

  if (entries.empty()) {
    throw MakeFinanceError(
        "finance_invalid_watchlist",
        "Watchlist must contain at least one stock code.",
        std::string(raw_value));
  }

  if (entries.size() > 5) {
    entries.resize(5);
  }
  return entries;
}

JsonValue ParseJsonResponse(std::string_view body,
                            std::string code,
                            std::string message) {
  auto parsed = JsonValue::Parse(body, std::move(code), std::move(message), 6);
  if (std::holds_alternative<AppError>(parsed)) {
    throw std::get<AppError>(parsed);
  }
  return std::get<JsonValue>(std::move(parsed));
}

JsonValue HttpGetJson(std::string_view url) {
  const auto response = HttpGet(url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "finance_request_failed";
    error.message = "Finance data request failed.";
    error.hint = std::string(url) + " | " + error.hint;
    throw error;
  }
  return ParseJsonResponse(std::get<std::string>(response),
                           "finance_parse_failed",
                           "Finance response could not be parsed.");
}

double SignedNumber(const JsonValue& value, std::string_view rf) {
  double amount = value.AsDouble(0.0);
  if (amount < 0.0) {
    return amount;
  }
  if (rf == "5") {
    return -amount;
  }
  if (rf == "2") {
    return amount;
  }
  return amount == 0.0 ? 0.0 : amount;
}

std::string MarketStateLabel(std::string_view raw) {
  std::string token;
  token.reserve(raw.size());
  for (unsigned char ch : raw) {
    token.push_back(static_cast<char>(std::toupper(ch)));
  }
  if (token == "CLOSE") {
    return "장마감";
  }
  if (token.find("OPEN") != std::string::npos) {
    return "장중";
  }
  if (token.find("AFTER") != std::string::npos) {
    return "시간외";
  }
  return "시장 상태 확인";
}

std::string FormatNumber(double value, int decimals = 2) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(decimals) << value;
  std::string text = stream.str();
  const std::size_t dot = text.find('.');
  const std::size_t end = dot == std::string::npos ? text.size() : dot;
  for (std::ptrdiff_t index = static_cast<std::ptrdiff_t>(end) - 3; index > 0;
       index -= 3) {
    text.insert(static_cast<std::size_t>(index), ",");
  }
  return text;
}

std::string FormatSigned(double value, int decimals = 2, std::string_view suffix = "") {
  std::ostringstream stream;
  if (value > 0.0) {
    stream << '+';
  }
  stream << FormatNumber(value, decimals) << suffix;
  return stream.str();
}

std::string FormatStockPrice(double value) {
  return FormatNumber(value, 0) + "원";
}

std::string DirectionIcon(double value) {
  if (value > 0.0) {
    return "arrowUpward";
  }
  if (value < 0.0) {
    return "arrowDownward";
  }
  return "showChart";
}

bool IsAscii(std::string_view value) {
  return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
    return ch < 0x80;
  });
}

std::optional<std::string> ConvertToUtf8(std::string_view value,
                                         const char* source_encoding) {
  iconv_t converter = iconv_open("UTF-8", source_encoding);
  if (converter == reinterpret_cast<iconv_t>(-1)) {
    return std::nullopt;
  }

  std::string output(value.size() * 4 + 16, '\0');
  char* input_buffer = const_cast<char*>(value.data());
  std::size_t input_left = value.size();
  char* output_buffer = output.data();
  std::size_t output_left = output.size();

  while (input_left > 0) {
    if (iconv(converter, &input_buffer, &input_left, &output_buffer,
              &output_left) == static_cast<std::size_t>(-1)) {
      iconv_close(converter);
      return std::nullopt;
    }
  }

  iconv_close(converter);
  output.resize(output.size() - output_left);
  return output;
}

std::string DecodeNaverLabel(std::string_view raw_value) {
  if (raw_value.empty()) {
    return {};
  }
  if (IsAscii(raw_value)) {
    return CleanText(raw_value, 24);
  }

  for (const char* encoding : {"CP949", "EUC-KR"}) {
    const auto converted = ConvertToUtf8(raw_value, encoding);
    if (converted.has_value() && !converted->empty()) {
      return CleanText(*converted, 24);
    }
  }

  return {};
}

std::string ResolveWatchLabel(const WatchlistEntry& entry,
                              const JsonValue& quote) {
  if (!entry.label.empty() && entry.label != entry.code) {
    return entry.label;
  }

  const std::string decoded_name = DecodeNaverLabel(quote.At("nm").AsString(""));
  if (!decoded_name.empty()) {
    return decoded_name;
  }

  return entry.code;
}

std::string BuildNaverQueryUrl(std::string_view query) {
  return std::string(kNaverRealtimeBase) + UrlEncode(query);
}

JsonValue ExtractNaverQuote(std::string_view query) {
  const JsonValue payload = HttpGetJson(BuildNaverQueryUrl(query));
  const JsonValue result = payload.At("result");
  const JsonValue first_area = result.At("areas").At(0);
  const JsonValue first_quote = first_area.At("datas").At(0);
  if (first_quote.IsNull()) {
    throw MakeFinanceError(
        "finance_invalid_response",
        "Naver finance response did not include quote data.",
        std::string(query));
  }
  return first_quote;
}

MarketSnapshot FetchIndexSnapshot(std::string_view code) {
  const JsonValue quote =
      ExtractNaverQuote("SERVICE_INDEX:" + std::string(code));
  const std::string rf = quote.At("rf").AsString("");
  return MarketSnapshot{
      std::string(code),
      std::string(code),
      quote.At("nv").AsDouble(0.0) / 100.0,
      SignedNumber(quote.At("cv"), rf) / 100.0,
      SignedNumber(quote.At("cr"), rf),
      MarketStateLabel(quote.At("ms").AsString("")),
  };
}

MarketSnapshot FetchStockSnapshot(const WatchlistEntry& entry) {
  const JsonValue quote =
      ExtractNaverQuote("SERVICE_ITEM:" + entry.code);
  const std::string rf = quote.At("rf").AsString("");
  return MarketSnapshot{
      entry.code,
      ResolveWatchLabel(entry, quote),
      quote.At("nv").AsDouble(0.0),
      SignedNumber(quote.At("cv"), rf),
      SignedNumber(quote.At("cr"), rf),
      MarketStateLabel(quote.At("ms").AsString("")),
  };
}

std::string OffsetDate(std::string_view date_text, int day_offset) {
  std::tm tm = {};
  std::istringstream stream{std::string(date_text)};
  stream >> std::get_time(&tm, "%Y-%m-%d");
  if (stream.fail()) {
    throw MakeFinanceError(
        "finance_invalid_date",
        "Finance exchange API returned an invalid date.",
        std::string(date_text));
  }
  tm.tm_mday += day_offset;
  const std::time_t timestamp = std::mktime(&tm);
  if (timestamp == static_cast<std::time_t>(-1)) {
    throw MakeFinanceError(
        "finance_invalid_date",
        "Finance exchange API returned an unusable date.",
        std::string(date_text));
  }
  const std::tm* normalized = std::localtime(&timestamp);
  if (normalized == nullptr) {
    throw MakeFinanceError(
        "finance_invalid_date",
        "Failed to normalize exchange history date.",
        std::string(date_text));
  }
  std::ostringstream out;
  out << std::put_time(normalized, "%Y-%m-%d");
  return out.str();
}

ExchangeSnapshot FetchExchangeSnapshot() {
  const JsonValue latest =
      HttpGetJson(std::string(kFrankfurterBase) +
                  "/latest?base=KRW&symbols=USD,JPY");
  const std::string latest_date = latest.At("date").AsString("");
  const JsonValue latest_rates = latest.At("rates");
  const double usd_latest = latest_rates.At("USD").AsDouble(0.0);
  const double jpy_latest = latest_rates.At("JPY").AsDouble(0.0);
  if (latest_date.empty() || usd_latest == 0.0 || jpy_latest == 0.0) {
    throw MakeFinanceError(
        "finance_invalid_response",
        "Frankfurter latest response is missing exchange rates.",
        latest.Dump(false));
  }

  const std::string start_date = OffsetDate(latest_date, -7);
  const JsonValue history = HttpGetJson(std::string(kFrankfurterBase) + "/" +
                                        start_date + ".." + latest_date +
                                        "?base=KRW&symbols=USD,JPY");
  const JsonValue rates_by_date = history.At("rates");
  if (!rates_by_date.IsObject()) {
    throw MakeFinanceError(
        "finance_invalid_response",
        "Frankfurter history response is missing rates.",
        history.Dump(false));
  }

  std::vector<std::string> dates;
  for (int days_back = 1; days_back <= 7; ++days_back) {
    dates.push_back(OffsetDate(latest_date, -days_back));
  }

  JsonValue previous_rates;
  for (const auto& date : dates) {
    previous_rates = rates_by_date.At(date);
    if (!previous_rates.IsNull()) {
      break;
    }
  }
  if (previous_rates.IsNull()) {
    previous_rates = latest_rates;
  }

  const double usd_previous = previous_rates.At("USD").AsDouble(usd_latest);
  const double jpy_previous = previous_rates.At("JPY").AsDouble(jpy_latest);
  if (usd_previous == 0.0 || jpy_previous == 0.0) {
    throw MakeFinanceError(
        "finance_invalid_response",
        "Frankfurter history response is missing previous exchange rates.",
        history.Dump(false));
  }

  const double usd_krw = 1.0 / usd_latest;
  const double usd_krw_prev = 1.0 / usd_previous;
  const double jpy100_krw = 100.0 / jpy_latest;
  const double jpy100_krw_prev = 100.0 / jpy_previous;

  return ExchangeSnapshot{
      latest_date,
      usd_krw,
      usd_krw - usd_krw_prev,
      jpy100_krw,
      jpy100_krw - jpy100_krw_prev,
  };
}

const MarketSnapshot* StrongestMover(const std::vector<MarketSnapshot>& stocks) {
  if (stocks.empty()) {
    return nullptr;
  }
  return &*std::max_element(
      stocks.begin(), stocks.end(), [](const MarketSnapshot& left,
                                       const MarketSnapshot& right) {
        return std::abs(left.change_pct) < std::abs(right.change_pct);
      });
}

JsonValue BuildFinancePayload(const std::vector<WatchlistEntry>& watchlist,
                              const MarketSnapshot* kospi,
                              const MarketSnapshot* kosdaq,
                              const ExchangeSnapshot* fx,
                              const std::vector<MarketSnapshot>& stocks,
                              const std::vector<std::string>& errors) {
  if (kospi == nullptr && fx == nullptr && stocks.empty()) {
    throw MakeFinanceError(
        "finance_no_data",
        "No usable finance cards could be built from live sources.",
        "Check Naver polling and Frankfurter connectivity.");
  }

  int rising_count = 0;
  for (const auto& stock : stocks) {
    if (stock.change_pct > 0.0) {
      ++rising_count;
    }
  }
  const MarketSnapshot* mover = StrongestMover(stocks);

  std::vector<std::string> headline_parts;
  if (kospi != nullptr) {
    headline_parts.push_back(
        CleanText("코스피 " + FormatNumber(kospi->current, 2) + " (" +
                      FormatSigned(kospi->change_pct, 2, "%") + ")",
                  34));
  }
  if (fx != nullptr) {
    headline_parts.push_back(
        CleanText("원/달러 " + FormatNumber(fx->usd_krw, 2) + "원", 24));
  }
  if (!stocks.empty()) {
    headline_parts.push_back("관심종목 " + std::to_string(rising_count) + " / " +
                             std::to_string(stocks.size()) + " 상승");
  }

  JsonValue primary_metrics = JsonValue::Array();
  ArrayAppend(
      primary_metrics,
      MakeObject({
          {"label", JsonValue::String("코스피")},
          {"value",
           JsonValue::String(kospi == nullptr ? "-" : FormatNumber(kospi->current, 2))},
          {"detail",
           JsonValue::String(
               kospi == nullptr
                   ? "국내 지수 연결 필요"
                   : CleanText(FormatSigned(kospi->change, 2, "p") + " · " +
                                   FormatSigned(kospi->change_pct, 2, "%") + " · " +
                                   kospi->status,
                               40))},
      }));
  ArrayAppend(
      primary_metrics,
      MakeObject({
          {"label", JsonValue::String("USD/KRW")},
          {"value",
           JsonValue::String(fx == nullptr ? "-" : FormatNumber(fx->usd_krw, 2))},
          {"detail",
           JsonValue::String(
               fx == nullptr
                   ? "환율 연결 필요"
                   : CleanText("전일 대비 " +
                                   FormatSigned(fx->usd_krw_change, 2) + " · " +
                                   fx->date + " 기준",
                               40))},
      }));
  ArrayAppend(
      primary_metrics,
      MakeObject({
          {"label", JsonValue::String("관심종목")},
          {"value",
           JsonValue::String(std::to_string(rising_count) + " / " +
                             std::to_string(stocks.size()) + " 상승")},
          {"detail",
           JsonValue::String(
               mover == nullptr
                   ? "관심종목 연결 필요"
                   : CleanText("최대 변동 " + mover->label + " " +
                                   FormatSigned(mover->change_pct, 2, "%"),
                               36))},
      }));

  JsonValue watch_items = JsonValue::Array();
  for (std::size_t index = 0; index < stocks.size() && index < 3; ++index) {
    const auto& stock = stocks[index];
    ArrayAppend(
        watch_items,
        MakeObject({
            {"icon", JsonValue::String(DirectionIcon(stock.change_pct))},
            {"label", JsonValue::String(stock.label)},
            {"value", JsonValue::String(FormatSigned(stock.change_pct, 2, "%"))},
            {"detail",
             JsonValue::String(
                 CleanText(FormatStockPrice(stock.current) + " · " + stock.status,
                           48))},
        }));
  }

  JsonValue indicator_items = JsonValue::Array();
  if (kospi != nullptr) {
    ArrayAppend(
        indicator_items,
        MakeObject({
            {"icon", JsonValue::String(DirectionIcon(kospi->change_pct))},
            {"label", JsonValue::String("코스피")},
            {"value", JsonValue::String(FormatNumber(kospi->current, 2))},
            {"detail",
             JsonValue::String(
                 CleanText(FormatSigned(kospi->change_pct, 2, "%") + " · " +
                               kospi->status,
                           36))},
        }));
  }
  if (kosdaq != nullptr) {
    ArrayAppend(
        indicator_items,
        MakeObject({
            {"icon", JsonValue::String(DirectionIcon(kosdaq->change_pct))},
            {"label", JsonValue::String("코스닥")},
            {"value", JsonValue::String(FormatNumber(kosdaq->current, 2))},
            {"detail",
             JsonValue::String(
                 CleanText(FormatSigned(kosdaq->change_pct, 2, "%") + " · " +
                               kosdaq->status,
                           36))},
        }));
  }
  if (fx != nullptr) {
    ArrayAppend(
        indicator_items,
        MakeObject({
            {"icon", JsonValue::String("currencyExchange")},
            {"label", JsonValue::String("USD/KRW")},
            {"value", JsonValue::String(FormatNumber(fx->usd_krw, 2))},
            {"detail",
             JsonValue::String(CleanText(
                 "전일 대비 " + FormatSigned(fx->usd_krw_change, 2) + " · " +
                     fx->date,
                 40))},
        }));
    ArrayAppend(
        indicator_items,
        MakeObject({
            {"icon", JsonValue::String("currencyExchange")},
            {"label", JsonValue::String("100JPY/KRW")},
            {"value", JsonValue::String(FormatNumber(fx->jpy100_krw, 2))},
            {"detail",
             JsonValue::String(CleanText(
                 "전일 대비 " + FormatSigned(fx->jpy100_krw_change, 2) + " · " +
                     fx->date,
                 40))},
        }));
  }

  JsonValue sections = JsonValue::Array();
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("관심종목")},
          {"items", std::move(watch_items)},
      }));
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("참고 지표")},
          {"items", std::move(indicator_items)},
      }));

  const std::string requested_labels = [&watchlist, &stocks]() {
    std::ostringstream joined;
    for (std::size_t index = 0; index < watchlist.size(); ++index) {
      std::string label = watchlist[index].label;
      if (label.empty() || label == watchlist[index].code) {
        const auto match =
            std::find_if(stocks.begin(), stocks.end(), [&](const auto& stock) {
              return stock.code == watchlist[index].code && !stock.label.empty();
            });
        if (match != stocks.end()) {
          label = match->label;
        }
      }
      if (index > 0) {
        joined << ", ";
      }
      joined << (label.empty() ? watchlist[index].code : label);
    }
    return joined.str();
  }();

  const bool partial = !errors.empty();
  std::string error_summary;
  for (std::size_t index = 0; index < errors.size(); ++index) {
    if (index > 0) {
      error_summary += " / ";
    }
    error_summary += errors[index];
  }

  return MakeObject({
      {"domain", JsonValue::String("finance")},
      {"source", JsonValue::String("naver-public")},
      {"title", JsonValue::String("시장 스냅샷")},
      {"headline",
       JsonValue::String(headline_parts.empty()
                             ? "KRW 기준 환율과 관심종목 움직임을 짧게 정리했습니다."
                             : CleanText(
                                   [&headline_parts]() {
                                     std::ostringstream joined;
                                     for (std::size_t index = 0;
                                          index < headline_parts.size(); ++index) {
                                       if (index > 0) {
                                         joined << " · ";
                                       }
                                       joined << headline_parts[index];
                                     }
                                     return joined.str();
                                   }(),
                                   88))},
      {"primaryMetrics", std::move(primary_metrics)},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String(partial ? "warning" : "gavel")},
           {"title", JsonValue::String(partial ? "일부 지표만 표시 중"
                                               : "투자 참고용 요약")},
           {"summary",
            JsonValue::String(
                partial ? CleanText(error_summary, 100)
                        : "TV 금융 스냅샷은 참고용 정보로만 유지하고, 추천이나 매수 유도처럼 보이는 표현은 피하는 편이 안전합니다.")},
           {"meta", JsonValue::String("Naver Finance · Frankfurter")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshFinance")},
           }),
           MakeObject({
               {"label", JsonValue::String("환율 보기")},
               {"event", JsonValue::String("showExchangeRates")},
           }),
       })},
      {"footer",
       JsonValue::String(
           CleanText("출처: Naver Finance, Frankfurter. 요청 watchlist: " +
                         requested_labels +
                         ". 장중/장마감 상태와 고시일을 함께 확인하는 편이 안전합니다.",
                     120))},
  });
}

}  // namespace

JsonResult LoadMockFinancePayload() {
  return LoadFixturePayload("mock_finance.json", "finance", "mock");
}

JsonResult Execute(const FinanceCommand& command) {
  if (command.source == FinanceCommand::Source::kMock) {
    if (command.dry_run) {
      const auto path = ResolveFixturePath("mock_finance.json");
      return MakeObject({
          {"command", JsonValue::String("finance")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           path.empty() ? JsonValue::Null() : JsonValue::String(path.string())},
      });
    }
    return LoadMockFinancePayload();
  }

  std::vector<WatchlistEntry> watchlist;
  try {
    watchlist = ParseWatchlist(command.watchlist);
  } catch (const AppError& error) {
    return error;
  }

  if (command.dry_run) {
    JsonValue watch_urls = JsonValue::Array();
    for (const auto& entry : watchlist) {
      ArrayAppend(
          watch_urls,
          JsonValue::String(BuildNaverQueryUrl("SERVICE_ITEM:" + entry.code)));
    }
    return MakeObject({
        {"command", JsonValue::String("finance")},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("naver-public")},
        {"request",
         MakeObject({
             {"watchlist", JsonValue::String(command.watchlist)},
             {"naver_urls",
              MakeArray({
                  JsonValue::String(BuildNaverQueryUrl("SERVICE_INDEX:KOSPI")),
                  JsonValue::String(BuildNaverQueryUrl("SERVICE_INDEX:KOSDAQ")),
              })},
             {"watch_urls", std::move(watch_urls)},
             {"frankfurter_latest",
              JsonValue::String(std::string(kFrankfurterBase) +
                                "/latest?base=KRW&symbols=USD,JPY")},
             {"frankfurter_history_hint",
              JsonValue::String(
                  "tizen-tool-domain-fetch requests a 7-day KRW history window to compute daily changes.")},
         })},
    });
  }

  try {
    std::vector<std::string> errors;
    std::optional<MarketSnapshot> kospi;
    std::optional<MarketSnapshot> kosdaq;
    std::optional<ExchangeSnapshot> fx;
    std::vector<MarketSnapshot> stocks;

    try {
      kospi = FetchIndexSnapshot("KOSPI");
    } catch (const AppError& error) {
      errors.push_back("코스피 연결 실패: " + CleanText(error.message, 34));
    }
    try {
      kosdaq = FetchIndexSnapshot("KOSDAQ");
    } catch (const AppError&) {
    }
    try {
      fx = FetchExchangeSnapshot();
    } catch (const AppError& error) {
      errors.push_back("환율 연결 실패: " + CleanText(error.message, 34));
    }
    for (const auto& entry : watchlist) {
      try {
        stocks.push_back(FetchStockSnapshot(entry));
      } catch (const AppError& error) {
        errors.push_back(entry.label + " 연결 실패: " + CleanText(error.message, 28));
      }
    }

    return BuildFinancePayload(
        watchlist, kospi ? &*kospi : nullptr, kosdaq ? &*kosdaq : nullptr,
        fx ? &*fx : nullptr, stocks, errors);
  } catch (const AppError& error) {
    return error;
  }
}

}  // namespace tizen_tool_domain_fetch::finance
