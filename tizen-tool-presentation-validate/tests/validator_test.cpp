#include <cassert>
#include <iostream>
#include <string>

#include "tizen_tool_presentation_validate/validator.hpp"

namespace {

using tizen_tool_presentation_validate::Severity;
using tizen_tool_presentation_validate::Validate;

const std::string kValidMinimal = R"({
  "surfaceId": "weather_today",
  "theme": {"domain": "weather", "pattern": "immersive"},
  "title": "서울 서초구",
  "hero": {"label": "현재 기온", "value": "-1°"}
})";

void TestValidMinimal() {
  const auto report = Validate(kValidMinimal, "minimal.json");
  assert(report.passed);
  assert(report.errors == 0);
  std::cout << "PASS TestValidMinimal\n";
}

void TestMarkdownWrappedPayload() {
  const std::string input = "```json\n" + kValidMinimal + "\n```";
  const auto report = Validate(input, "wrapped.json");
  assert(!report.passed);
  assert(report.checks[0].rule == "payload_structure");
  std::cout << "PASS TestMarkdownWrappedPayload\n";
}

void TestJsonLabeledPayload() {
  const std::string input = "json\n" + kValidMinimal;
  const auto report = Validate(input, "labeled.json");
  assert(!report.passed);
  assert(report.checks[0].rule == "payload_structure");
  std::cout << "PASS TestJsonLabeledPayload\n";
}

void TestLegacyA2uiPayloadRejected() {
  const auto report = Validate(
      R"({"version":"v0.9","createSurface":{"surfaceId":"weather"}})",
      "legacy.json");
  assert(!report.passed);
  assert(report.checks[0].message.find("legacy A2UI") != std::string::npos);
  std::cout << "PASS TestLegacyA2uiPayloadRejected\n";
}

void TestMissingRequiredField() {
  const auto report = Validate(
      R"({"theme":{"domain":"weather","pattern":"immersive"},"title":"x","hero":{"label":"A","value":"B"}})",
      "missing_surface_id.json");
  assert(!report.passed);
  bool found = false;
  for (const auto& check : report.checks) {
    if (check.rule == "required_fields" && !check.passed) {
      found = true;
    }
  }
  assert(found);
  std::cout << "PASS TestMissingRequiredField\n";
}

void TestChartLabelMismatch() {
  const auto report = Validate(
      R"({"surfaceId":"finance_focus","theme":{"domain":"finance","pattern":"centerCard"},"title":"삼성전자","hero":{"label":"현재가","value":"74,300원"},"chart":{"title":"추이","kind":"line","labels":["월","화"],"values":[1,2,3]}})",
      "chart_mismatch.json");
  assert(!report.passed);
  bool found = false;
  for (const auto& check : report.checks) {
    if (check.rule == "chart" && !check.passed) {
      found = true;
    }
  }
  assert(found);
  std::cout << "PASS TestChartLabelMismatch\n";
}

void TestFooterRejected() {
  const auto report = Validate(
      R"({"surfaceId":"finance_focus","theme":{"domain":"finance","pattern":"centerCard"},"title":"삼성전자","hero":{"label":"현재가","value":"74,300원"},"footer":"remove me"})",
      "footer.json");
  assert(!report.passed);
  bool found = false;
  for (const auto& check : report.checks) {
    if (check.rule == "forbidden_fields" && !check.passed) {
      found = true;
    }
  }
  assert(found);
  std::cout << "PASS TestFooterRejected\n";
}

void TestDensityWarning() {
  const auto report = Validate(
      R"({"surfaceId":"finance_focus","theme":{"domain":"finance","pattern":"centerCard"},"title":"삼성전자","hero":{"label":"현재가","value":"74,300원"},"metrics":[{"label":"a","value":"1"},{"label":"b","value":"2"},{"label":"c","value":"3"},{"label":"d","value":"4"},{"label":"e","value":"5"}]})",
      "dense.json");
  assert(report.passed);
  assert(report.warnings > 0);
  bool found = false;
  for (const auto& check : report.checks) {
    if (check.rule == "tv_density" && !check.passed &&
        check.severity == Severity::kWarning) {
      found = true;
    }
  }
  assert(found);
  std::cout << "PASS TestDensityWarning\n";
}

}  // namespace

int main() {
  TestValidMinimal();
  TestMarkdownWrappedPayload();
  TestJsonLabeledPayload();
  TestLegacyA2uiPayloadRejected();
  TestMissingRequiredField();
  TestChartLabelMismatch();
  TestFooterRejected();
  TestDensityWarning();
  std::cout << "tizen_tool_presentation_validate_tests passed\n";
  return 0;
}
