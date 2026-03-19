#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "tv_presentation_validate/json.hpp"

namespace tv_presentation_validate {

enum class Severity { kError, kWarning };

struct CheckResult {
  bool passed;
  Severity severity;
  std::string rule;
  std::string message;
  std::string detail;
};

struct ValidationReport {
  std::string file;
  bool passed = true;
  int errors = 0;
  int warnings = 0;
  int metric_count = 0;
  int fact_count = 0;
  int chart_point_count = 0;
  std::vector<CheckResult> checks;
};

ValidationReport Validate(std::string_view content, std::string_view file_name);

JsonValue ReportToJson(const ValidationReport& report);
std::string RenderPrettyReport(const ValidationReport& report);

}  // namespace tv_presentation_validate
