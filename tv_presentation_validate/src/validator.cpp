#include "tv_presentation_validate/validator.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace tv_presentation_validate {

namespace {

constexpr std::array<std::string_view, 5> kThemePatterns = {
    "immersive", "sidePanel", "centerCard", "topBanner", "bottomRibbon",
};

constexpr std::array<std::string_view, 3> kThemeScales = {
    "compact", "standard", "expanded",
};

constexpr std::array<std::string_view, 2> kChartKinds = {"line", "bar"};

template <typename Container>
bool Contains(const Container& container, std::string_view value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

std::string_view Trim(std::string_view content) {
  const auto start = content.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return {};
  }

  const auto end = content.find_last_not_of(" \t\r\n");
  return content.substr(start, end - start + 1);
}

bool StartsWith(std::string_view content, std::string_view prefix) {
  return content.size() >= prefix.size() &&
         content.substr(0, prefix.size()) == prefix;
}

bool LooksLikeMarkdownWrappedPayload(std::string_view content) {
  return StartsWith(content, "```") || StartsWith(content, "'''");
}

bool LooksLikeJsonLabeledPayload(std::string_view content) {
  if (!StartsWith(content, "json") && !StartsWith(content, "JSON")) {
    return false;
  }

  if (content.size() == 4) {
    return true;
  }

  const char next = content[4];
  return next == '\n' || next == '\r' || next == ' ' || next == '\t';
}

bool LooksLikeA2uiEnvelope(const JsonValue& json) {
  return ObjectHasKey(json, "version") || ObjectHasKey(json, "createSurface") ||
         ObjectHasKey(json, "updateDataModel") ||
         ObjectHasKey(json, "updateComponents") ||
         ObjectHasKey(json, "deleteSurface");
}

void AddPass(std::vector<CheckResult>& checks, std::string rule,
             std::string message) {
  checks.push_back({
      .passed = true,
      .severity = Severity::kError,
      .rule = std::move(rule),
      .message = std::move(message),
      .detail = {},
  });
}

void AddError(std::vector<CheckResult>& checks, std::string rule,
              std::string message, std::string detail = {}) {
  checks.push_back({
      .passed = false,
      .severity = Severity::kError,
      .rule = std::move(rule),
      .message = std::move(message),
      .detail = std::move(detail),
  });
}

void AddWarning(std::vector<CheckResult>& checks, std::string rule,
                std::string message, std::string detail = {}) {
  checks.push_back({
      .passed = false,
      .severity = Severity::kWarning,
      .rule = std::move(rule),
      .message = std::move(message),
      .detail = std::move(detail),
  });
}

bool IsNonEmptyString(const JsonValue& value) {
  return value.IsString() && !Trim(value.AsString()).empty();
}

std::string JoinStringViews(const auto& array, std::string_view sep) {
  std::string result;
  for (std::size_t i = 0; i < array.size(); ++i) {
    if (i > 0) {
      result += sep;
    }
    result += array[i];
  }
  return result;
}

bool ValidateLabelValueObject(const JsonValue& value, std::string_view path,
                              std::string& detail_out) {
  if (!value.IsObject()) {
    detail_out += std::string(path) + " item must be an object. ";
    return false;
  }

  bool ok = true;
  if (!IsNonEmptyString(value.At("label"))) {
    detail_out += std::string(path) + ".label must be a non-empty string. ";
    ok = false;
  }
  if (!IsNonEmptyString(value.At("value"))) {
    detail_out += std::string(path) + ".value must be a non-empty string. ";
    ok = false;
  }
  if (!value.At("detail").IsNull() && !value.At("detail").IsString()) {
    detail_out += std::string(path) + ".detail must be a string when present. ";
    ok = false;
  }
  return ok;
}

int CountErrors(const std::vector<CheckResult>& checks) {
  return static_cast<int>(std::count_if(
      checks.begin(), checks.end(), [](const auto& check) {
        return !check.passed && check.severity == Severity::kError;
      }));
}

int CountWarnings(const std::vector<CheckResult>& checks) {
  return static_cast<int>(std::count_if(
      checks.begin(), checks.end(), [](const auto& check) {
        return !check.passed && check.severity == Severity::kWarning;
      }));
}

}  // namespace

ValidationReport Validate(std::string_view content,
                          std::string_view file_name) {
  ValidationReport report;
  report.file = std::string(file_name);

  const auto trimmed = Trim(content);
  if (trimmed.empty()) {
    AddError(report.checks, "payload_structure", "Input is empty.");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  if (LooksLikeMarkdownWrappedPayload(trimmed)) {
    AddError(report.checks, "payload_structure",
             "Received Markdown code fences instead of raw JSON.",
             "Remove wrappers like ```json ... ``` and pass one raw JSON object.");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  if (LooksLikeJsonLabeledPayload(trimmed)) {
    AddError(report.checks, "payload_structure",
             "Received a payload prefixed with a json label.",
             "Remove the leading 'json' label and pass one raw JSON object.");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  auto parsed = JsonValue::Parse(trimmed, "parse_error",
                                 "Input is not valid JSON.", 1);
  if (std::holds_alternative<AppError>(parsed)) {
    AddError(report.checks, "payload_structure",
             "Input is not valid JSON.",
             std::get<AppError>(parsed).hint);
    report.errors = 1;
    report.passed = false;
    return report;
  }

  auto payload = std::get<JsonValue>(parsed);
  if (!payload.IsObject()) {
    AddError(report.checks, "payload_structure",
             "Presentation payload must be one JSON object.",
             "Do not wrap the payload in a JSON array or NDJSON envelope.");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  if (LooksLikeA2uiEnvelope(payload)) {
    AddError(report.checks, "payload_structure",
             "Received a legacy A2UI payload instead of presentation JSON.",
             "Pass the semantic presentation object, not createSurface/updateDataModel/updateComponents.");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  AddPass(report.checks, "payload_structure",
          "One raw presentation JSON object.");

  // Required root fields.
  std::string required_detail;
  if (!IsNonEmptyString(payload.At("surfaceId"))) {
    required_detail += "surfaceId is required. ";
  }
  if (!payload.At("theme").IsObject()) {
    required_detail += "theme must be an object. ";
  }
  if (!IsNonEmptyString(payload.At("title"))) {
    required_detail += "title is required. ";
  }
  if (!payload.At("hero").IsObject()) {
    required_detail += "hero must be an object. ";
  }

  if (required_detail.empty()) {
    AddPass(report.checks, "required_fields",
            "surfaceId, theme, title, and hero are present.");
  } else {
    AddError(report.checks, "required_fields",
             "Missing or invalid required root fields.", required_detail);
  }

  // Theme.
  auto theme = payload.At("theme");
  if (!theme.IsObject()) {
    AddError(report.checks, "theme", "theme must be an object.");
  } else {
    const auto domain = Trim(theme.At("domain").AsString());
    const auto pattern = Trim(theme.At("pattern").AsString());
    const auto scale = Trim(theme.At("scale").AsString());
    if (domain.empty()) {
      AddError(report.checks, "theme", "theme.domain is missing.");
    } else if (pattern.empty()) {
      AddError(report.checks, "theme", "theme.pattern is missing.");
    } else if (!Contains(kThemePatterns, pattern)) {
      AddError(report.checks, "theme",
               "Invalid theme.pattern: \"" + std::string(pattern) + "\".",
               "Valid: immersive, sidePanel, centerCard, topBanner, bottomRibbon.");
    } else if (!scale.empty() && !Contains(kThemeScales, scale)) {
      AddError(report.checks, "theme",
               "Invalid theme.scale: \"" + std::string(scale) + "\".",
               "Valid: compact, standard, expanded.");
    } else {
      AddPass(report.checks, "theme",
              "domain=\"" + std::string(domain) + "\", pattern=\"" +
                  std::string(pattern) + "\".");
    }
  }

  // Hero.
  auto hero = payload.At("hero");
  if (!hero.IsObject()) {
    AddError(report.checks, "hero", "hero must be an object.");
  } else {
    std::string hero_detail;
    if (!IsNonEmptyString(hero.At("label"))) {
      hero_detail += "hero.label must be a non-empty string. ";
    }
    if (!IsNonEmptyString(hero.At("value"))) {
      hero_detail += "hero.value must be a non-empty string. ";
    }
    if (!hero.At("detail").IsNull() && !hero.At("detail").IsString()) {
      hero_detail += "hero.detail must be a string when present. ";
    }
    if (!hero.At("caption").IsNull() && !hero.At("caption").IsString()) {
      hero_detail += "hero.caption must be a string when present. ";
    }
    if (hero_detail.empty()) {
      AddPass(report.checks, "hero",
              "hero.label and hero.value are valid.");
    } else {
      AddError(report.checks, "hero", "Invalid hero object.", hero_detail);
    }
  }

  if (!payload.At("summary").IsNull() && !payload.At("summary").IsString()) {
    AddError(report.checks, "summary", "summary must be a string when present.");
  } else {
    AddPass(report.checks, "summary", "summary is valid or omitted.");
  }

  // metrics and facts
  for (const auto [field_name, counter] :
       {std::pair<std::string_view, int*>("metrics", &report.metric_count),
        std::pair<std::string_view, int*>("facts", &report.fact_count)}) {
    auto list = payload.At(field_name);
    if (list.IsNull()) {
      AddPass(report.checks, std::string(field_name),
              std::string(field_name) + " omitted.");
      continue;
    }
    if (!list.IsArray()) {
      AddError(report.checks, std::string(field_name),
               std::string(field_name) + " must be an array when present.");
      continue;
    }

    *counter = static_cast<int>(list.Size());
    std::string detail;
    bool ok = true;
    for (std::size_t i = 0; i < list.Size(); ++i) {
      ok = ValidateLabelValueObject(
               list.At(i),
               std::string(field_name) + "[" + std::to_string(i) + "]",
               detail) &&
           ok;
    }
    if (ok) {
      AddPass(report.checks, std::string(field_name),
              std::to_string(list.Size()) + " valid " +
                  std::string(field_name) + " item(s).");
    } else {
      AddError(report.checks, std::string(field_name),
               "Invalid " + std::string(field_name) + " array.", detail);
    }
  }

  // chart
  auto chart = payload.At("chart");
  if (chart.IsNull()) {
    AddPass(report.checks, "chart", "chart omitted.");
  } else if (!chart.IsObject()) {
    AddError(report.checks, "chart", "chart must be an object when present.");
  } else {
    std::string detail;
    bool ok = true;
    if (!IsNonEmptyString(chart.At("title"))) {
      detail += "chart.title must be a non-empty string. ";
      ok = false;
    }
    const auto kind = Trim(chart.At("kind").AsString());
    if (kind.empty() || !Contains(kChartKinds, kind)) {
      detail += "chart.kind must be line or bar. ";
      ok = false;
    }

    auto values = chart.At("values");
    if (!values.IsArray()) {
      detail += "chart.values must be an array of numbers. ";
      ok = false;
    } else {
      report.chart_point_count = static_cast<int>(values.Size());
      if (values.Size() < 2) {
        detail += "chart.values must contain at least 2 points. ";
        ok = false;
      }
      for (std::size_t i = 0; i < values.Size(); ++i) {
        if (!values.At(i).IsNumber()) {
          detail += "chart.values[" + std::to_string(i) +
                    "] must be numeric. ";
          ok = false;
        }
      }
    }

    auto labels = chart.At("labels");
    if (!labels.IsNull()) {
      if (!labels.IsArray()) {
        detail += "chart.labels must be an array of strings. ";
        ok = false;
      } else {
        if (values.IsArray() && labels.Size() != values.Size()) {
          detail += "chart.labels length must match chart.values length. ";
          ok = false;
        }
        for (std::size_t i = 0; i < labels.Size(); ++i) {
          if (!IsNonEmptyString(labels.At(i))) {
            detail += "chart.labels[" + std::to_string(i) +
                      "] must be a non-empty string. ";
            ok = false;
          }
        }
      }
    }

    if (!chart.At("unitLabel").IsNull() && !chart.At("unitLabel").IsString()) {
      detail += "chart.unitLabel must be a string when present. ";
      ok = false;
    }
    if (!chart.At("detail").IsNull() && !chart.At("detail").IsString()) {
      detail += "chart.detail must be a string when present. ";
      ok = false;
    }

    if (ok) {
      AddPass(report.checks, "chart",
              std::string(kind) + " chart with " +
                  std::to_string(report.chart_point_count) + " point(s).");
    } else {
      AddError(report.checks, "chart", "Invalid chart object.", detail);
    }
  }

  // alert
  auto alert = payload.At("alert");
  if (alert.IsNull()) {
    AddPass(report.checks, "alert", "alert omitted.");
  } else if (!alert.IsObject()) {
    AddError(report.checks, "alert", "alert must be an object when present.");
  } else {
    std::string detail;
    if (!IsNonEmptyString(alert.At("title"))) {
      detail += "alert.title must be a non-empty string. ";
    }
    if (!IsNonEmptyString(alert.At("summary"))) {
      detail += "alert.summary must be a non-empty string. ";
    }
    if (!alert.At("meta").IsNull() && !alert.At("meta").IsString()) {
      detail += "alert.meta must be a string when present. ";
    }
    if (detail.empty()) {
      AddPass(report.checks, "alert", "alert object is valid.");
    } else {
      AddError(report.checks, "alert", "Invalid alert object.", detail);
    }
  }

  // forbidden fields
  std::string forbidden_detail;
  if (ObjectHasKey(payload, "footer")) {
    forbidden_detail += "footer is not allowed. ";
  }
  if (ObjectHasKey(payload, "createSurface") ||
      ObjectHasKey(payload, "updateDataModel") ||
      ObjectHasKey(payload, "updateComponents")) {
    forbidden_detail += "legacy A2UI fields are not allowed. ";
  }
  if (forbidden_detail.empty()) {
    AddPass(report.checks, "forbidden_fields",
            "No forbidden footer or legacy A2UI fields.");
  } else {
    AddError(report.checks, "forbidden_fields",
             "Forbidden fields are present.", forbidden_detail);
  }

  // TV density warnings
  std::string density_detail;
  if (report.metric_count > 4) {
    density_detail += "metrics exceeds 4 items. ";
  }
  if (report.fact_count > 4) {
    density_detail += "facts exceeds 4 items. ";
  }
  if (report.chart_point_count > 0 &&
      (report.chart_point_count < 2 || report.chart_point_count > 8)) {
    density_detail += "chart should prefer 2-8 points for TV readability. ";
  }
  if (density_detail.empty()) {
    AddPass(report.checks, "tv_density",
            "metrics/facts density looks TV-friendly.");
  } else {
    AddWarning(report.checks, "tv_density",
               "Presentation may be dense for a 10-foot TV UI.",
               density_detail);
  }

  report.errors = CountErrors(report.checks);
  report.warnings = CountWarnings(report.checks);
  report.passed = report.errors == 0;
  return report;
}

JsonValue ReportToJson(const ValidationReport& report) {
  JsonValue document = JsonValue::Object();
  ObjectSet(document, "ok", JsonValue::Boolean(report.passed));
  ObjectSet(document, "file", JsonValue::String(report.file));
  ObjectSet(document, "errors", JsonValue::Integer(report.errors));
  ObjectSet(document, "warnings", JsonValue::Integer(report.warnings));
  ObjectSet(document, "metric_count", JsonValue::Integer(report.metric_count));
  ObjectSet(document, "fact_count", JsonValue::Integer(report.fact_count));
  ObjectSet(document, "chart_point_count",
            JsonValue::Integer(report.chart_point_count));

  JsonValue checks = JsonValue::Array();
  for (const auto& check : report.checks) {
    JsonValue item = JsonValue::Object();
    ObjectSet(item, "passed", JsonValue::Boolean(check.passed));
    ObjectSet(item, "severity",
              JsonValue::String(check.severity == Severity::kWarning ? "warning"
                                                                    : "error"));
    ObjectSet(item, "rule", JsonValue::String(check.rule));
    ObjectSet(item, "message", JsonValue::String(check.message));
    if (!check.detail.empty()) {
      ObjectSet(item, "detail", JsonValue::String(check.detail));
    }
    ArrayAppend(checks, std::move(item));
  }
  ObjectSet(document, "checks", std::move(checks));
  return document;
}

std::string RenderPrettyReport(const ValidationReport& report) {
  std::ostringstream stream;
  stream << (report.passed ? "PASS" : "FAIL") << " " << report.file << "\n";
  stream << "errors=" << report.errors << " warnings=" << report.warnings
         << " metrics=" << report.metric_count << " facts=" << report.fact_count
         << " chart_points=" << report.chart_point_count << "\n";

  for (const auto& check : report.checks) {
    stream << (check.passed ? "[ok] " :
               (check.severity == Severity::kWarning ? "[warn] " : "[error] "))
           << check.rule << ": " << check.message << "\n";
    if (!check.detail.empty()) {
      stream << "  " << check.detail << "\n";
    }
  }
  return stream.str();
}

}  // namespace tv_presentation_validate
