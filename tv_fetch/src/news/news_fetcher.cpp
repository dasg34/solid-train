#include "tv_fetch/news/news_fetcher.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tv_fetch/http_client.hpp"
#include "tv_fetch/support.hpp"

namespace tv_fetch::news {

namespace {

struct FeedItem {
  std::string title;
  std::string link;
  std::string label;
  std::string detail;
  bool is_breaking = false;
};

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
  if (value.starts_with(kPrefix) && value.ends_with(kSuffix) &&
      value.size() >= kPrefix.size() + kSuffix.size()) {
    value.remove_prefix(kPrefix.size());
    value.remove_suffix(kSuffix.size());
  }
  return DecodeXmlEntities(std::string(value));
}

FeedItem NormalizeFeedItem(std::string_view item_xml) {
  const std::string title =
      CleanText(StripCdata(ExtractTag(item_xml, "title").value_or("")), 88);
  const std::string link =
      CleanText(StripCdata(ExtractTag(item_xml, "link").value_or("")), 120);
  const std::string category =
      CleanText(StripCdata(ExtractTag(item_xml, "category").value_or("최신")), 20);
  const std::string published =
      CleanText(StripCdata(ExtractTag(item_xml, "pubDate").value_or("")), 44);
  const bool is_breaking = title.find("[속보]") != std::string::npos;

  return FeedItem{
      .title = title,
      .link = link,
      .label = is_breaking ? "속보" : category,
      .detail = published.empty() ? "시각 확인 필요"
                                  : CleanText(published + " · 연합뉴스TV", 52),
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
        {"source", JsonValue::String("yonhap-rss")},
        {"request",
         MakeObject({
             {"rss_url", JsonValue::String(command.rss_url)},
             {"count", JsonValue::Integer(command.count)},
         })},
    });
  }

  const auto response = HttpGet(command.rss_url);
  if (std::holds_alternative<AppError>(response)) {
    AppError error = std::get<AppError>(response);
    error.code = "news_request_failed";
    error.message = "News RSS request failed.";
    error.hint = command.rss_url + " | " + error.hint;
    return error;
  }

  try {
    return NormalizeYonhapRss(std::get<std::string>(response), command.count);
  } catch (const AppError& error) {
    return error;
  }
}

}  // namespace tv_fetch::news
