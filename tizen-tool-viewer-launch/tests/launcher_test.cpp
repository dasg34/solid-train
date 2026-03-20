#include <cstdlib>
#include <iostream>
#include <sstream>

#include "tizen_tool_viewer_launch/cli.hpp"
#include "tizen_tool_viewer_launch/launcher.hpp"

namespace {

void Assert(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "Assertion failed: " << message << '\n';
    std::exit(1);
  }
}

void TestParseCommand() {
  const char* argv[] = {
      "tizen-tool-viewer-launch", "--file", "/tmp/presentation.json", "--app-id",
      "org.tizen.tizen-tool-viewer", "--dry-run", "--format", "pretty"};
  const auto parsed =
      tizen_tool_viewer_launch::ParseCommand(8, const_cast<char**>(argv));
  Assert(std::holds_alternative<tizen_tool_viewer_launch::LaunchCommand>(parsed),
         "launcher command should parse");
  const auto& command =
      std::get<tizen_tool_viewer_launch::LaunchCommand>(parsed);
  Assert(command.input_file == "/tmp/presentation.json",
         "input file should parse");
  Assert(command.app_id == "org.tizen.tizen-tool-viewer",
         "app id should parse");
  Assert(command.dry_run, "dry-run should parse");
  Assert(command.format == tizen_tool_viewer_launch::OutputFormat::kPretty,
         "format should parse");
}

void TestPreparePayloadFromStdin() {
  tizen_tool_viewer_launch::LaunchCommand command;
  std::istringstream input(
      "{\"surfaceId\":\"finance_focus\","
      "\"theme\":{\"domain\":\"finance\",\"pattern\":\"centerCard\"},"
      "\"title\":\"삼성전자\","
      "\"hero\":{\"label\":\"현재가\",\"value\":\"74,300원\"}}\n");

  const auto prepared = tizen_tool_viewer_launch::PreparePayload(command, input);
  Assert(
      std::holds_alternative<tizen_tool_viewer_launch::PersistedPayload>(prepared),
         "payload should persist");
  const auto& payload =
      std::get<tizen_tool_viewer_launch::PersistedPayload>(prepared);
  Assert(payload.used_stdin, "stdin flag should be true");
  Assert(payload.source_label == "stdin", "source label should identify stdin");
  Assert(payload.json.find("\"surfaceId\":\"finance_focus\"") !=
             std::string::npos,
         "prepared payload should keep the presentation JSON content");
}

void TestDryRunLaunch() {
  tizen_tool_viewer_launch::LaunchCommand command;
  command.dry_run = true;

  tizen_tool_viewer_launch::PersistedPayload payload{
      .json =
          "{\"surfaceId\":\"weather_today\","
          "\"theme\":{\"domain\":\"weather\",\"pattern\":\"immersive\"},"
          "\"title\":\"서울 서초구\","
          "\"hero\":{\"label\":\"현재 기온\",\"value\":\"-1°\"}}\n",
      .source_label = "stdin",
      .bytes = 42,
      .used_stdin = true,
  };

  const auto launched = tizen_tool_viewer_launch::LaunchPayload(command, payload);
  Assert(std::holds_alternative<tizen_tool_viewer_launch::LaunchReport>(launched),
         "dry-run launch should not fail");
  const auto& report =
      std::get<tizen_tool_viewer_launch::LaunchReport>(launched);
  Assert(!report.launched, "dry-run should not send launch request");
  Assert(report.bytes == 42, "report should preserve byte count");
  Assert(report.source_label == "stdin", "report should preserve source label");
  Assert(!report.replayed_after_launch,
         "dry-run should not mark cold-start replay");
  Assert(report.replay_delay_ms == 0,
         "dry-run should not report replay delay");
}

}  // namespace

int main() {
  TestParseCommand();
  TestPreparePayloadFromStdin();
  TestDryRunLaunch();
  std::cout << "tizen-tool-viewer-launch_tests passed\n";
  return 0;
}
