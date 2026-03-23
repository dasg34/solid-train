#include "tizen_tool_domain_fetch/news/news_fetcher.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tizen_tool_domain_fetch/http_client.hpp"
#include "tizen_tool_domain_fetch/support.hpp"

namespace tizen_tool_domain_fetch::news {

namespace {

bool StartsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
  return value.size() >= suffix.size() &&
         value.substr(value.size() - suffix.size()) == suffix;
}

struct FeedItem {
  std::string title;
  std::string link;
  std::string label;
  std::string detail;
  std::string publisher;
  bool is_breaking = false;
};

std::string GoogleNewsSearchUrl(std::string_view query) {
  return "https://news.google.com/rss/search?q=" + UrlEncode(query) +
         "&hl=ko&gl=KR&ceid=KR:ko";
}

std::optional<std::string> ExtractTag(std::string_view text,
                                      std::string_view tag,
                                      std::size_t start = 0) {
  const std::string open = "<" + std::string(tag);
  const std::string close = "</" + std::string(tag) + ">";
  const std::size_t open_pos = text.find(open, start);
  if (open_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t value_start = text.find('>', open_pos);
  if (value_start == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t close_pos = text.find(close, value_start + 1);
  if (close_pos == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(text.substr(value_start + 1, close_pos - value_start - 1));
}

std::vector<std::string> ExtractBlocks(std::string_view text, std::string_view tag) {
  const std::string open = "<" + std::string(tag);
  const std::string close = "</" + std::string(tag) + ">";
  std::vector<std::string> blocks;
  std::size_t cursor = 0;
  while (true) {
    const std::size_t open_pos = text.find(open, cursor);
    if (open_pos == std::string_view::npos) {
      break;
    }
    const std::size_t value_start = text.find('>', open_pos);
    if (value_start == std::string_view::npos) {
      break;
    }
    const std::size_t close_pos = text.find(close, value_start + 1);
    if (close_pos == std::string_view::npos) {
      break;
    }
    blocks.emplace_back(
        text.substr(value_start + 1, close_pos - value_start - 1));
    cursor = close_pos + close.size();
  }
  return blocks;
}

std::string DecodeXmlEntities(std::string value) {
  const std::vector<std::pair<std::string, std::string>> replacements = {
      {"&amp;", "&"},
      {"&lt;", "<"},
      {"&gt;", ">"},
      {"&quot;", "\""},
      {"&apos;", "'"},
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

std::string StripCdata(std::string_view value) {
  constexpr std::string_view kPrefix = "<![CDATA[";
  constexpr std::string_view kSuffix = "]]>";
  if (StartsWith(value, kPrefix) && EndsWith(value, kSuffix) &&
      value.size() >= kPrefix.size() + kSuffix.size()) {
    value.remove_prefix(kPrefix.size());
    value.remove_suffix(kSuffix.size());
  }
  return DecodeXmlEntities(std::string(value));
}

FeedItem NormalizeFeedItem(std::string_view item_xml) {
  std::string title = StripCdata(ExtractTag(item_xml, "title").value_or(""));
  const std::string link =
      CleanText(StripCdata(ExtractTag(item_xml, "link").value_or("")), 120);
  const std::string category =
      CleanText(StripCdata(ExtractTag(item_xml, "category").value_or("최신")), 20);
  const std::string publisher =
      CleanText(StripCdata(ExtractTag(item_xml, "source").value_or("")), 28);
  if (!publisher.empty()) {
    const std::string suffix = " - " + publisher;
    if (title.size() > suffix.size() && EndsWith(title, suffix)) {
      title.erase(title.size() - suffix.size());
    }
  }
  title = CleanText(title, 120);
  const std::string published =
      CleanText(StripCdata(ExtractTag(item_xml, "pubDate").value_or("")), 44);
  const bool is_breaking = title.find("[속보]") != std::string::npos;

  return FeedItem{
      .title = title,
      .link = link,
      .label = is_breaking ? "속보" : category,
      .detail = published.empty()
                    ? (publisher.empty() ? "시각 확인 필요"
                                         : CleanText("시각 확인 필요 · " + publisher, 80))
                    : CleanText(published + (publisher.empty() ? "" : " · " + publisher),
                                80),
      .publisher = publisher,
      .is_breaking = is_breaking,
  };
}

JsonValue NormalizeYonhapRss(std::string_view xml_text, int count) {
  const auto last_build =
      CleanText(StripCdata(ExtractTag(xml_text, "lastBuildDate").value_or("")), 40);
  const auto item_blocks = ExtractBlocks(xml_text, "item");
  if (item_blocks.empty()) {
    throw AppError{
        .code = "news_empty_feed",
        .message = "Yonhap RSS feed did not contain any items.",
        .hint = "Check the RSS source or retry later.",
        .exit_code = 6,
    };
  }

  std::vector<FeedItem> items;
  items.reserve(static_cast<std::size_t>(count));
  for (const auto& item_block : item_blocks) {
    if (static_cast<int>(items.size()) >= count) {
      break;
    }
    items.push_back(NormalizeFeedItem(item_block));
  }
  if (items.empty()) {
    throw AppError{
        .code = "news_empty_feed",
        .message = "Yonhap RSS feed returned no usable items.",
        .hint = "Check the RSS XML structure.",
        .exit_code = 6,
    };
  }

  JsonValue lead_items = JsonValue::Array();
  ArrayAppend(
      lead_items,
      MakeObject({
          {"icon", JsonValue::String("article")},
          {"label", JsonValue::String(items.front().label)},
          {"value", JsonValue::String(items.front().title)},
          {"detail", JsonValue::String(items.front().detail)},
      }));

  JsonValue sections = JsonValue::Array();
  ArrayAppend(
      sections,
      MakeObject({
          {"title", JsonValue::String("리드 스토리")},
          {"items", std::move(lead_items)},
      }));

  if (items.size() > 1) {
    JsonValue secondary_items = JsonValue::Array();
    for (std::size_t index = 1; index < items.size() && index < 6; ++index) {
      ArrayAppend(
          secondary_items,
          MakeObject({
              {"icon", JsonValue::String("article")},
              {"label", JsonValue::String(items[index].label)},
              {"value", JsonValue::String(items[index].title)},
              {"detail", JsonValue::String(items[index].detail)},
          }));
    }
    ArrayAppend(
        sections,
        MakeObject({
            {"title", JsonValue::String("최신 헤드라인")},
            {"items", std::move(secondary_items)},
        }));
  }

  int breaking_count = 0;
  for (const auto& item : items) {
    if (item.is_breaking) {
      ++breaking_count;
    }
  }

  return MakeObject({
      {"domain", JsonValue::String("news")},
      {"source", JsonValue::String("yonhap-rss")},
      {"updated_at",
       last_build.empty() ? JsonValue::Null() : JsonValue::String(last_build)},
      {"title", JsonValue::String("오늘의 주요 뉴스")},
      {"headline", JsonValue::String(items.front().title)},
      {"primaryMetrics",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("출처")},
               {"value", JsonValue::String("연합뉴스TV")},
               {"detail",
                JsonValue::String(last_build.empty() ? "갱신 시각 확인 필요"
                                                     : last_build + " 기준")},
           }),
           MakeObject({
               {"label", JsonValue::String("헤드라인")},
               {"value", JsonValue::String(std::to_string(items.size()) + "건")},
               {"detail", JsonValue::String("최신 RSS 피드")},
           }),
           MakeObject({
               {"label", JsonValue::String("속보")},
               {"value",
                JsonValue::String(std::to_string(breaking_count) + "건")},
               {"detail", JsonValue::String("제목 기준 속보 표기")},
           }),
       })},
      {"sections", std::move(sections)},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("info")},
           {"title", JsonValue::String("출처와 시각 유지")},
           {"summary",
            JsonValue::String(
                "라이브 뉴스 화면은 기사 원문 링크와 발행 또는 갱신 시각을 함께 표시하는 편이 안전합니다.")},
           {"meta", JsonValue::String("연합뉴스TV")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshNews")},
           }),
           MakeObject({
               {"label", JsonValue::String("리드 보기")},
               {"event", JsonValue::String("openLeadStory")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "요약 문장은 기사 본문을 과감하게 재서술하기보다 헤드라인과 시각 중심으로 짧게 유지합니다.")},
  });
}

JsonValue NormalizeGoogleNewsSearch(std::string_view xml_text,
                                    std::string_view query,
                                    int count) {
  const auto last_build =
      CleanText(StripCdata(ExtractTag(xml_text, "lastBuildDate").value_or("")), 40);
  const auto item_blocks = ExtractBlocks(xml_text, "item");
  if (item_blocks.empty()) {
    throw AppError{
        .code = "news_empty_feed",
        .message = "Google News search feed did not contain any items.",
        .hint = std::string(query),
        .exit_code = 6,
    };
  }

  std::vector<FeedItem> items;
  items.reserve(static_cast<std::size_t>(count));
  for (const auto& item_block : item_blocks) {
    if (static_cast<int>(items.size()) >= count) {
      break;
    }
    items.push_back(NormalizeFeedItem(item_block));
  }
  if (items.empty()) {
    throw AppError{
        .code = "news_empty_feed",
        .message = "Google News search returned no usable items.",
        .hint = std::string(query),
        .exit_code = 6,
    };
  }

  std::vector<std::string> publishers;
  for (const auto& item : items) {
    if (!item.publisher.empty() &&
        std::find(publishers.begin(), publishers.end(), item.publisher) ==
            publishers.end()) {
      publishers.push_back(item.publisher);
    }
  }

  JsonValue search_items = JsonValue::Array();
  for (const auto& item : items) {
    ArrayAppend(
        search_items,
        MakeObject({
            {"icon", JsonValue::String("article")},
            {"label", JsonValue::String(item.publisher.empty() ? "검색 결과"
                                                               : item.publisher)},
            {"value", JsonValue::String(item.title)},
            {"detail", JsonValue::String(item.detail)},
        }));
  }

  std::string publisher_summary = "출처 확인 필요";
  if (!publishers.empty()) {
    publisher_summary = publishers.front();
    if (publishers.size() > 1) {
      publisher_summary += " 외 " + std::to_string(publishers.size() - 1) + "곳";
    }
  }

  return MakeObject({
      {"domain", JsonValue::String("news")},
      {"source", JsonValue::String("google-news-rss")},
      {"query", JsonValue::String(query)},
      {"updated_at",
       last_build.empty() ? JsonValue::Null() : JsonValue::String(last_build)},
      {"title", JsonValue::String("뉴스 검색 결과")},
      {"headline", JsonValue::String(items.front().title)},
      {"primaryMetrics",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("검색어")},
               {"value", JsonValue::String(CleanText(query, 18))},
               {"detail", JsonValue::String("Google 뉴스 RSS")},
           }),
           MakeObject({
               {"label", JsonValue::String("결과")},
               {"value", JsonValue::String(std::to_string(items.size()) + "건")},
               {"detail",
                JsonValue::String(last_build.empty() ? "갱신 시각 확인 필요"
                                                     : last_build + " 기준")},
           }),
           MakeObject({
               {"label", JsonValue::String("출처")},
               {"value", JsonValue::String(std::to_string(publishers.size()) + "곳")},
               {"detail", JsonValue::String(CleanText(publisher_summary, 32))},
           }),
       })},
      {"sections",
       MakeArray({
           MakeObject({
               {"title", JsonValue::String("검색 결과")},
               {"items", std::move(search_items)},
           }),
       })},
      {"alert",
       MakeObject({
           {"icon", JsonValue::String("info")},
           {"title", JsonValue::String("검색 출처와 시각 유지")},
           {"summary",
            JsonValue::String(
                "검색 결과는 여러 언론사를 혼합하므로 기사 제목과 발행 시각, 출처를 함께 표시하는 편이 안전합니다.")},
           {"meta", JsonValue::String("Google News RSS")},
       })},
      {"actions",
       MakeArray({
           MakeObject({
               {"label", JsonValue::String("새로고침")},
               {"event", JsonValue::String("refreshNews")},
           }),
           MakeObject({
               {"label", JsonValue::String("검색 유지")},
               {"event", JsonValue::String("refineNewsQuery")},
           }),
       })},
      {"footer",
       JsonValue::String(
           "뉴스 검색은 여러 출처를 섞어 보여주므로, TV 화면에서는 사실 전달 중심의 짧은 헤드라인 요약으로 유지하는 편이 좋습니다.")},
  });
}

}  // namespace

JsonResult LoadMockNewsPayload() {
  return LoadFixturePayload("mock_news.json", "news", "mock");
}

JsonResult Execute(const NewsCommand& command) {
  if (command.source == NewsCommand::Source::kMock) {
    if (command.dry_run) {
      const auto path = ResolveFixturePath("mock_news.json");
      return MakeObject({
          {"command", JsonValue::String("news")},
          {"mode", JsonValue::String("dry-run")},
          {"source", JsonValue::String("mock")},
          {"fixture_path",
           path.empty() ? JsonValue::Null() : JsonValue::String(path.string())},
      });
    }
    return LoadMockNewsPayload();
  }

  if (command.dry_run) {
    return MakeObject({
        {"command", JsonValue::String("news")},
        {"mode", JsonValue::String("dry-run")},
        {"source",
         JsonValue::String(command.source == NewsCommand::Source::kGoogleNewsRss
                               ? "google-news-rss"
                               : "yonhap-rss")},
        {"request",
         MakeObject({
             {"rss_url",
              JsonValue::String(command.source == NewsCommand::Source::kGoogleNewsRss
                                    ? GoogleNewsSearchUrl(command.query)
                                    : command.rss_url)},
             {"query",
              command.query.empty() ? JsonValue::Null()
                                    : JsonValue::String(command.query)},
             {"count", JsonValue::Integer(command.count)},
         })},
    });
  }

  const std::string rss_url =
      command.source == NewsCommand::Source::kGoogleNewsRss
          ? GoogleNewsSearchUrl(command.query)
          : command.rss_url;

  const auto response = HttpGet(rss_url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "news_request_failed";
    error.message = "News RSS request failed.";
    error.hint = rss_url + " | " + error.hint;
    return error;
  }

  try {
    if (command.source == NewsCommand::Source::kGoogleNewsRss) {
      return NormalizeGoogleNewsSearch(std::get<std::string>(response),
                                       command.query, command.count);
    }
    return NormalizeYonhapRss(std::get<std::string>(response), command.count);
  } catch (const AppError& error) {
    return error;
  }
}

}  // namespace tizen_tool_domain_fetch::news
