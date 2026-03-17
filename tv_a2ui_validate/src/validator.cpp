#include "tv_a2ui_validate/validator.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace tv_a2ui_validate {

namespace {

constexpr std::array<std::string_view, 11> kComponentTypes = {
    "Text",      "Column",   "Row",      "Inset",  "Wrap",    "Card",
    "Icon",      "Divider",  "Button",   "LineChart", "BarChart",
};

constexpr std::array<std::string_view, 5> kThemePatterns = {
    "immersive", "sidePanel", "centerCard", "topBanner", "bottomRibbon",
};

constexpr std::array<std::string_view, 48> kValidIcons = {
    "accountCircle",  "add",            "arrowBack",      "arrowForward",
    "attachFile",     "calendarToday",  "call",           "camera",
    "check",          "close",          "delete",         "download",
    "edit",           "error",          "event",          "favorite",
    "favoriteOff",    "folder",         "help",           "home",
    "info",           "locationOn",     "lock",           "lockOpen",
    "mail",           "menu",           "moreHoriz",      "moreVert",
    "notifications",  "notificationsOff", "payment",      "person",
    "phone",          "photo",          "print",          "refresh",
    "search",         "send",           "settings",       "share",
    "shoppingCart",   "star",           "starHalf",       "starOff",
    "upload",         "visibility",     "visibilityOff",  "warning",
};

constexpr int kMaxComponents = 40;

template <typename Container>
bool Contains(const Container& container, std::string_view value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

std::vector<std::string> SplitLines(std::string_view content) {
  std::vector<std::string> lines;
  std::istringstream stream{std::string(content)};
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      lines.push_back(std::move(line));
    }
  }
  return lines;
}

void AddPass(std::vector<CheckResult>& checks, std::string rule,
             std::string message) {
  checks.push_back({
      .passed = true,
      .severity = Severity::kError,
      .rule = std::move(rule),
      .message = std::move(message),
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

std::string PathToKey(std::string_view path) {
  if (!path.empty() && path[0] == '/') {
    return std::string(path.substr(1));
  }
  return std::string(path);
}

struct PathRef {
  std::string path;
  std::string component_id;
  std::string property;
};

struct ChildRef {
  std::string target_id;
  std::string source_id;
  std::string property;
};

void CollectDataPaths(const JsonValue& component, const std::string& comp_id,
                      std::vector<PathRef>& paths) {
  const std::array<std::string_view, 3> kPathProperties = {
      "text", "values", "labels",
  };
  for (const auto& prop : kPathProperties) {
    auto value = component.At(prop);
    if (value.IsObject() && ObjectHasKey(value, "path")) {
      paths.push_back({value.At("path").AsString(), comp_id,
                        std::string(prop)});
    }
  }
  auto children = component.At("children");
  if (children.IsObject() && ObjectHasKey(children, "path")) {
    paths.push_back({children.At("path").AsString(), comp_id, "children"});
  }
}

void CollectChildRefs(const JsonValue& component, const std::string& comp_id,
                      std::vector<ChildRef>& refs) {
  auto child = component.At("child");
  if (!child.IsNull()) {
    refs.push_back({child.AsString(), comp_id, "child"});
  }

  auto children = component.At("children");
  if (children.IsArray()) {
    for (std::size_t i = 0; i < children.Size(); ++i) {
      auto item = children.At(i);
      if (!item.IsNull()) {
        refs.push_back({item.AsString(), comp_id, "children"});
      }
    }
  } else if (children.IsObject() && ObjectHasKey(children, "componentId")) {
    refs.push_back(
        {children.At("componentId").AsString(), comp_id, "children"});
  }
}

std::string JoinStringViews(const auto& array, std::string_view sep) {
  std::string result;
  for (std::size_t i = 0; i < array.size(); ++i) {
    if (i > 0) result += sep;
    result += array[i];
  }
  return result;
}

}  // namespace

ValidationReport Validate(std::string_view content,
                          std::string_view file_name) {
  ValidationReport report;
  report.file = std::string(file_name);

  const auto lines = SplitLines(content);

  // Check 1: NDJSON structure — exactly 3 non-empty lines.
  if (lines.size() != 3) {
    AddError(report.checks, "ndjson_structure",
             "Expected 3 NDJSON lines, got " + std::to_string(lines.size()) +
                 ".");
    report.errors = 1;
    report.passed = false;
    return report;
  }

  // Parse each line.
  auto parsed0 = JsonValue::Parse(lines[0], "parse_error",
                                  "Line 1 is not valid JSON.", 1);
  auto parsed1 = JsonValue::Parse(lines[1], "parse_error",
                                  "Line 2 is not valid JSON.", 1);
  auto parsed2 = JsonValue::Parse(lines[2], "parse_error",
                                  "Line 3 is not valid JSON.", 1);

  if (std::holds_alternative<AppError>(parsed0) ||
      std::holds_alternative<AppError>(parsed1) ||
      std::holds_alternative<AppError>(parsed2)) {
    std::string detail;
    if (std::holds_alternative<AppError>(parsed0)) {
      detail += "Line 1: " + std::get<AppError>(parsed0).hint;
    }
    if (std::holds_alternative<AppError>(parsed1)) {
      if (!detail.empty()) detail += "; ";
      detail += "Line 2: " + std::get<AppError>(parsed1).hint;
    }
    if (std::holds_alternative<AppError>(parsed2)) {
      if (!detail.empty()) detail += "; ";
      detail += "Line 3: " + std::get<AppError>(parsed2).hint;
    }
    AddError(report.checks, "ndjson_structure",
             "One or more lines are not valid JSON.", detail);
    report.errors = 1;
    report.passed = false;
    return report;
  }

  AddPass(report.checks, "ndjson_structure", "3 valid JSON lines.");

  auto& msg0 = std::get<JsonValue>(parsed0);
  auto& msg1 = std::get<JsonValue>(parsed1);
  auto& msg2 = std::get<JsonValue>(parsed2);

  // Check 2: message order.
  const bool has_create = ObjectHasKey(msg0, "createSurface");
  const bool has_data = ObjectHasKey(msg1, "updateDataModel");
  const bool has_components = ObjectHasKey(msg2, "updateComponents");

  if (has_create && has_data && has_components) {
    AddPass(report.checks, "message_order",
            "createSurface, updateDataModel, updateComponents.");
  } else {
    std::string detail;
    if (!has_create) detail += "Line 1 missing createSurface. ";
    if (!has_data) detail += "Line 2 missing updateDataModel. ";
    if (!has_components) detail += "Line 3 missing updateComponents.";
    AddError(report.checks, "message_order",
             "Messages not in expected order.", detail);
    report.errors = 1;
    report.passed = false;
    return report;
  }

  // Extract key structures.
  auto create_surface = msg0.At("createSurface");
  auto update_data = msg1.At("updateDataModel");
  auto update_components = msg2.At("updateComponents");

  // Check 3: surfaceId consistency.
  const std::string sid0 = create_surface.At("surfaceId").AsString();
  const std::string sid1 = update_data.At("surfaceId").AsString();
  const std::string sid2 = update_components.At("surfaceId").AsString();

  if (sid0.empty()) {
    AddError(report.checks, "surface_id",
             "createSurface.surfaceId is missing.");
  } else if (sid0 == sid1 && sid0 == sid2) {
    AddPass(report.checks, "surface_id",
            "\"" + sid0 + "\" (consistent).");
  } else {
    AddError(report.checks, "surface_id",
             "surfaceId mismatch.",
             "createSurface=" + sid0 + " updateDataModel=" + sid1 +
                 " updateComponents=" + sid2);
  }

  // Check 4: theme.
  auto theme = create_surface.At("theme");
  if (theme.IsNull() || !theme.IsObject()) {
    AddError(report.checks, "theme", "createSurface.theme is missing.",
             "Add \"theme\": {\"domain\": \"<domain>\", \"pattern\": "
             "\"<immersive|sidePanel|centerCard|topBanner|bottomRibbon>\"} "
             "to createSurface.");
  } else {
    const std::string domain = theme.At("domain").AsString();
    const std::string pattern = theme.At("pattern").AsString();

    if (domain.empty()) {
      AddError(report.checks, "theme", "theme.domain is missing.");
    } else if (pattern.empty()) {
      AddError(report.checks, "theme", "theme.pattern is missing.");
    } else if (!Contains(kThemePatterns, pattern)) {
      AddError(report.checks, "theme",
               "Invalid theme.pattern: \"" + pattern + "\".",
               "Valid: immersive, sidePanel, centerCard, topBanner, "
               "bottomRibbon.");
    } else {
      AddPass(report.checks, "theme",
              "domain=\"" + domain + "\", pattern=\"" + pattern + "\".");
    }
  }

  // Extract components and data model.
  auto components = update_components.At("components");
  auto data_value = update_data.At("value");

  if (!components.IsArray()) {
    AddError(report.checks, "root_component",
             "updateComponents.components is not an array.");
    report.errors =
        static_cast<int>(std::count_if(
            report.checks.begin(), report.checks.end(),
            [](const auto& c) {
              return !c.passed && c.severity == Severity::kError;
            }));
    report.passed = report.errors == 0;
    return report;
  }

  report.total_components = static_cast<int>(components.Size());

  // Build ID set and check types/icons.
  std::set<std::string> defined_ids;
  std::vector<std::string> invalid_types;
  std::vector<std::string> invalid_icons;
  std::vector<ChildRef> child_refs;
  std::vector<PathRef> data_paths;
  bool has_root = false;

  for (std::size_t i = 0; i < components.Size(); ++i) {
    auto comp = components.At(i);
    const std::string id = comp.At("id").AsString();
    const std::string type = comp.At("component").AsString();

    if (!id.empty()) {
      defined_ids.insert(id);
    }
    if (id == "root") {
      has_root = true;
    }
    if (!type.empty() && !Contains(kComponentTypes, type)) {
      invalid_types.push_back(type + " (id=" + id + ")");
    }
    if (type == "Icon") {
      const std::string name = comp.At("name").AsString();
      if (!name.empty() && !Contains(kValidIcons, name)) {
        invalid_icons.push_back(name + " (id=" + id + ")");
      }
    }

    CollectChildRefs(comp, id, child_refs);
    CollectDataPaths(comp, id, data_paths);
  }

  // Check 5: root component.
  if (has_root) {
    AddPass(report.checks, "root_component", "Root component exists.");
  } else {
    AddError(report.checks, "root_component",
             "No component with id=\"root\" found.");
  }

  // Check 6: component types.
  if (invalid_types.empty()) {
    AddPass(report.checks, "component_types",
            "All " + std::to_string(report.total_components) +
                " components use valid types.");
  } else {
    std::string detail;
    for (const auto& entry : invalid_types) {
      if (!detail.empty()) detail += ", ";
      detail += entry;
    }
    detail += ". Valid types: " + JoinStringViews(kComponentTypes, ", ") + ".";
    AddError(report.checks, "component_types",
             std::to_string(invalid_types.size()) +
                 " invalid component type(s).",
             detail);
  }

  // Check 7: icon names.
  if (invalid_icons.empty()) {
    AddPass(report.checks, "icon_names", "All icon names are valid.");
  } else {
    std::string detail;
    for (const auto& entry : invalid_icons) {
      if (!detail.empty()) detail += ", ";
      detail += entry;
    }
    detail += ". Valid icons: " + JoinStringViews(kValidIcons, ", ") + ".";
    AddError(report.checks, "icon_names",
             std::to_string(invalid_icons.size()) + " invalid icon name(s).",
             detail);
  }

  // Check 8: referential integrity.
  std::vector<std::string> dangling_details;
  for (const auto& ref : child_refs) {
    if (defined_ids.find(ref.target_id) == defined_ids.end()) {
      dangling_details.push_back("\"" + ref.target_id +
                                 "\" (referenced by id=\"" + ref.source_id +
                                 "\"." + ref.property + ")");
    }
  }

  if (dangling_details.empty()) {
    AddPass(report.checks, "referential_integrity",
            "All " + std::to_string(child_refs.size()) +
                " child/children references resolved.");
  } else {
    std::string detail;
    for (const auto& entry : dangling_details) {
      if (!detail.empty()) detail += "; ";
      detail += entry;
    }
    detail += ". Either add the missing component or fix the reference.";
    AddError(report.checks, "referential_integrity",
             std::to_string(dangling_details.size()) +
                 " dangling reference(s).",
             detail);
  }

  // Check 9: data bindings.
  std::set<std::string> data_keys;
  for (const auto& key : ObjectKeys(data_value)) {
    data_keys.insert(key);
  }

  std::vector<std::string> missing_details;
  for (const auto& ref : data_paths) {
    const std::string key = PathToKey(ref.path);
    if (data_keys.find(key) == data_keys.end()) {
      missing_details.push_back("\"" + ref.path + "\" (used by id=\"" +
                                ref.component_id + "\"." + ref.property + ")");
    }
  }

  if (missing_details.empty()) {
    AddPass(report.checks, "data_bindings",
            "All " + std::to_string(data_paths.size()) +
                " data path(s) found in dataModel.");
  } else {
    std::string detail;
    for (const auto& entry : missing_details) {
      if (!detail.empty()) detail += "; ";
      detail += entry;
    }
    detail += ". Either add the key to updateDataModel.value or "
              "change the component to use a static value.";
    AddError(report.checks, "data_bindings",
             std::to_string(missing_details.size()) +
                 " path(s) not found in dataModel.",
             detail);
  }

  // Check 10: component count.
  if (report.total_components <= kMaxComponents) {
    AddPass(report.checks, "component_count",
            std::to_string(report.total_components) + " components (limit: " +
                std::to_string(kMaxComponents) + ").");
  } else {
    AddWarning(report.checks, "component_count",
               std::to_string(report.total_components) +
                   " components exceeds recommended limit of " +
                   std::to_string(kMaxComponents) + ".",
               "TV surfaces with many components may be hard to read "
               "at 10-foot viewing distance.");
  }

  // Summarize.
  report.errors = static_cast<int>(std::count_if(
      report.checks.begin(), report.checks.end(),
      [](const auto& c) {
        return !c.passed && c.severity == Severity::kError;
      }));
  report.warnings = static_cast<int>(std::count_if(
      report.checks.begin(), report.checks.end(),
      [](const auto& c) {
        return !c.passed && c.severity == Severity::kWarning;
      }));
  report.passed = report.errors == 0;
  return report;
}

JsonValue ReportToJson(const ValidationReport& report) {
  JsonValue document = JsonValue::Object();
  ObjectSet(document, "file", JsonValue::String(report.file));
  ObjectSet(document, "passed", JsonValue::Boolean(report.passed));
  ObjectSet(document, "errors", JsonValue::Integer(report.errors));
  ObjectSet(document, "warnings", JsonValue::Integer(report.warnings));
  ObjectSet(document, "total_components",
            JsonValue::Integer(report.total_components));

  JsonValue checks = JsonValue::Array();
  for (const auto& check : report.checks) {
    JsonValue entry = JsonValue::Object();
    ObjectSet(entry, "passed", JsonValue::Boolean(check.passed));
    ObjectSet(entry, "severity",
              JsonValue::String(check.severity == Severity::kError ? "error"
                                                                   : "warning"));
    ObjectSet(entry, "rule", JsonValue::String(check.rule));
    ObjectSet(entry, "message", JsonValue::String(check.message));
    if (!check.detail.empty()) {
      ObjectSet(entry, "detail", JsonValue::String(check.detail));
    }
    ArrayAppend(checks, std::move(entry));
  }
  ObjectSet(document, "checks", std::move(checks));
  return document;
}

std::string RenderPrettyReport(const ValidationReport& report) {
  std::ostringstream stream;

  for (const auto& check : report.checks) {
    if (check.passed) {
      stream << "  PASS  " << check.rule << ": " << check.message << '\n';
    } else if (check.severity == Severity::kWarning) {
      stream << "  WARN  " << check.rule << ": " << check.message << '\n';
    } else {
      stream << "  FAIL  " << check.rule << ": " << check.message << '\n';
    }
    if (!check.detail.empty()) {
      stream << "        " << check.detail << '\n';
    }
  }

  stream << '\n';
  if (report.passed && report.warnings == 0) {
    stream << "Result: " << report.checks.size() << "/"
           << report.checks.size() << " passed.\n";
  } else if (report.passed) {
    stream << "Result: " << (report.checks.size() - static_cast<std::size_t>(
                                 report.errors + report.warnings))
           << "/" << report.checks.size() << " passed, "
           << report.warnings << " warning(s).\n";
  } else {
    stream << "Result: " << report.errors << " error(s), "
           << report.warnings << " warning(s).\n";
  }
  return stream.str();
}

}  // namespace tv_a2ui_validate
