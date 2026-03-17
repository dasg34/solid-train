#include <cassert>
#include <iostream>
#include <string>

#include "tv_a2ui_validate/validator.hpp"

namespace {

using tv_a2ui_validate::Severity;
using tv_a2ui_validate::Validate;

const std::string kValidMinimal =
    R"({"version":"v0.9","createSurface":{"surfaceId":"weather","catalogId":"https://a2ui.org/specification/v0_9/standard_catalog.json","theme":{"domain":"weather","pattern":"immersive"}}})"
    "\n"
    R"({"version":"v0.9","updateDataModel":{"surfaceId":"weather","value":{"title":"Seoul","temp":"18°"}}})"
    "\n"
    R"({"version":"v0.9","updateComponents":{"surfaceId":"weather","components":[{"id":"root","component":"Column","children":["t"]},{"id":"t","component":"Text","text":{"path":"/title"}}]}})";

void TestValidMinimal() {
  const auto report = Validate(kValidMinimal, "minimal.json");
  assert(report.passed);
  assert(report.errors == 0);
  assert(report.total_components == 2);
  std::cout << "PASS TestValidMinimal\n";
}

void TestBadLineCount() {
  const auto report = Validate("just one line", "bad.json");
  assert(!report.passed);
  assert(report.errors > 0);
  assert(report.checks[0].rule == "ndjson_structure");
  std::cout << "PASS TestBadLineCount\n";
}

void TestBadJson() {
  const std::string input = "not json\nnot json\nnot json\n";
  const auto report = Validate(input, "bad.json");
  assert(!report.passed);
  assert(report.checks[0].rule == "ndjson_structure");
  std::cout << "PASS TestBadJson\n";
}

void TestMissingRoot() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"noroot","component":"Text","text":"hi"}]}})";
  const auto report = Validate(input, "no_root.json");
  assert(!report.passed);

  bool found_root_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "root_component" && !check.passed) {
      found_root_fail = true;
    }
  }
  assert(found_root_fail);
  std::cout << "PASS TestMissingRoot\n";
}

void TestInvalidComponentType() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"root","component":"FancyWidget"}]}})";
  const auto report = Validate(input, "bad_type.json");
  assert(!report.passed);

  bool found_type_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "component_types" && !check.passed) {
      found_type_fail = true;
    }
  }
  assert(found_type_fail);
  std::cout << "PASS TestInvalidComponentType\n";
}

void TestInvalidIconName() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"root","component":"Column","children":["ic"]},{"id":"ic","component":"Icon","name":"wbSunny"}]}})";
  const auto report = Validate(input, "bad_icon.json");
  assert(!report.passed);

  bool found_icon_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "icon_names" && !check.passed) {
      found_icon_fail = true;
    }
  }
  assert(found_icon_fail);
  std::cout << "PASS TestInvalidIconName\n";
}

void TestSurfaceIdMismatch() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"alpha","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"beta","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"alpha","components":[{"id":"root","component":"Text","text":"x"}]}})";
  const auto report = Validate(input, "mismatch.json");
  assert(!report.passed);

  bool found_sid_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "surface_id" && !check.passed) {
      found_sid_fail = true;
    }
  }
  assert(found_sid_fail);
  std::cout << "PASS TestSurfaceIdMismatch\n";
}

void TestDanglingRef() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"root","component":"Column","children":["ghost"]}]}})";
  const auto report = Validate(input, "dangling.json");
  assert(!report.passed);

  bool found_ref_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "referential_integrity" && !check.passed) {
      found_ref_fail = true;
    }
  }
  assert(found_ref_fail);
  std::cout << "PASS TestDanglingRef\n";
}

void TestMissingDataBinding() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{"title":"ok"}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"root","component":"Text","text":{"path":"/missing_key"}}]}})";
  const auto report = Validate(input, "bad_binding.json");
  assert(!report.passed);

  bool found_binding_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "data_bindings" && !check.passed) {
      found_binding_fail = true;
    }
  }
  assert(found_binding_fail);
  std::cout << "PASS TestMissingDataBinding\n";
}

void TestComponentCountWarning() {
  std::string components = "[";
  components += R"({"id":"root","component":"Column","children":[)";
  for (int i = 0; i < 45; ++i) {
    if (i > 0) components += ",";
    components += "\"t" + std::to_string(i) + "\"";
  }
  components += "]}";
  for (int i = 0; i < 45; ++i) {
    components += R"(,{"id":"t)" + std::to_string(i) +
                  R"(","component":"Text","text":"x"})";
  }
  components += "]";

  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c","theme":{"domain":"d","pattern":"immersive"}}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":)" +
      components + "}}";

  const auto report = Validate(input, "many.json");
  assert(report.passed);
  assert(report.warnings > 0);

  bool found_count_warning = false;
  for (const auto& check : report.checks) {
    if (check.rule == "component_count" && !check.passed &&
        check.severity == Severity::kWarning) {
      found_count_warning = true;
    }
  }
  assert(found_count_warning);
  std::cout << "PASS TestComponentCountWarning\n";
}

void TestMissingTheme() {
  const std::string input =
      R"({"version":"v0.9","createSurface":{"surfaceId":"s","catalogId":"c"}})"
      "\n"
      R"({"version":"v0.9","updateDataModel":{"surfaceId":"s","value":{}}})"
      "\n"
      R"({"version":"v0.9","updateComponents":{"surfaceId":"s","components":[{"id":"root","component":"Text","text":"x"}]}})";
  const auto report = Validate(input, "no_theme.json");
  assert(!report.passed);

  bool found_theme_fail = false;
  for (const auto& check : report.checks) {
    if (check.rule == "theme" && !check.passed) {
      found_theme_fail = true;
    }
  }
  assert(found_theme_fail);
  std::cout << "PASS TestMissingTheme\n";
}

}  // namespace

int main() {
  TestValidMinimal();
  TestBadLineCount();
  TestBadJson();
  TestMissingRoot();
  TestInvalidComponentType();
  TestInvalidIconName();
  TestSurfaceIdMismatch();
  TestDanglingRef();
  TestMissingDataBinding();
  TestComponentCountWarning();
  TestMissingTheme();
  std::cout << "\nAll 11 tests passed.\n";
  return 0;
}
