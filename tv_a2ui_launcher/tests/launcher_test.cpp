#include <cstdlib>
#include <iostream>
#include <sstream>

#include "tv_a2ui_launcher/cli.hpp"
#include "tv_a2ui_launcher/launcher.hpp"

namespace {

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    std::exit(1);
  }
}

void TestParseCommand() {
  const char* argv[] = {
      "tv_a2ui_launcher", "--file", "/tmp/a2ui.json", "--app-id",
      "com.example_tv_genui", "--dry-run", "--format", "pretty"};
  const auto parsed =
      tv_a2ui_launcher::ParseCommand(8, const_cast<char**>(argv));
  Assert(std::holds_alternative<tv_a2ui_launcher::LaunchCommand>(parsed),
         "launcher command should parse");
  const auto& command =
      std::get<tv_a2ui_launcher::LaunchCommand>(parsed);
  Assert(command.input_file == "/tmp/a2ui.json", "input file should parse");
  Assert(command.app_id == "com.example_tv_genui", "app id should parse");
  Assert(command.dry_run, "dry-run should parse");
  Assert(command.format == tv_a2ui_launcher::OutputFormat::kPretty,
         "format should parse");
}

void TestPreparePayloadFromStdin() {
  tv_a2ui_launcher::LaunchCommand command;
  std::istringstream input(
      "{\"createSurface\":{\"surfaceId\":\"demo\"}}\n"
      "{\"updateDataModel\":{\"state\":{\"title\":\"hello\"}}}\n");

  const auto prepared = tv_a2ui_launcher::PreparePayload(command, input);
  Assert(std::holds_alternative<tv_a2ui_launcher::PersistedPayload>(prepared),
         "payload should persist");
  const auto& payload =
      std::get<tv_a2ui_launcher::PersistedPayload>(prepared);
  Assert(payload.used_stdin, "stdin flag should be true");
  Assert(payload.source_label == "stdin", "source label should identify stdin");
  Assert(payload.json.find("\"surfaceId\":\"demo\"") != std::string::npos,
         "prepared payload should keep the NDJSON content");
}

void TestDryRunLaunch() {
  tv_a2ui_launcher::LaunchCommand command;
  command.dry_run = true;

  tv_a2ui_launcher::PersistedPayload payload{
      .json = "{\"createSurface\":{\"surfaceId\":\"demo\"}}\n",
      .source_label = "stdin",
      .bytes = 42,
      .used_stdin = true,
  };

  const auto launched = tv_a2ui_launcher::LaunchPayload(command, payload);
  Assert(std::holds_alternative<tv_a2ui_launcher::LaunchReport>(launched),
         "dry-run launch should not fail");
  const auto& report =
      std::get<tv_a2ui_launcher::LaunchReport>(launched);
  Assert(!report.launched, "dry-run should not send launch request");
  Assert(report.bytes == 42, "report should preserve byte count");
  Assert(report.source_label == "stdin", "report should preserve source label");
}

}  // namespace

int main() {
  TestParseCommand();
  TestPreparePayloadFromStdin();
  TestDryRunLaunch();
  std::cout << "tv_a2ui_launcher_tests passed\n";
  return 0;
}
