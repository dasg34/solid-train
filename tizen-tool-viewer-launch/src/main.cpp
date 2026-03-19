#include <iostream>

#include "tizen_tool_viewer_launch/cli.hpp"
#include "tizen_tool_viewer_launch/launcher.hpp"

int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string_view arg1(argv[1]);
    if (arg1 == "--help" || arg1 == "-h") {
      std::cout << tizen_tool_viewer_launch::RenderHelp() << '\n';
      return 0;
    }
  }

  const auto parsed = tizen_tool_viewer_launch::ParseCommand(argc, argv);
  if (std::holds_alternative<tizen_tool_viewer_launch::AppError>(parsed)) {
    const auto& error = std::get<tizen_tool_viewer_launch::AppError>(parsed);
    if (error.code == "invalid_arguments" && error.message == "Help requested.") {
      std::cout << error.hint << '\n';
      return 0;
    }
    std::cerr << tizen_tool_viewer_launch::RenderError(
                     error, tizen_tool_viewer_launch::OutputFormat::kJson)
              << '\n';
    return error.exit_code;
  }

  const auto& command = std::get<tizen_tool_viewer_launch::LaunchCommand>(parsed);
  const auto prepared =
      tizen_tool_viewer_launch::PreparePayload(command, std::cin);
  if (std::holds_alternative<tizen_tool_viewer_launch::AppError>(prepared)) {
    const auto& error = std::get<tizen_tool_viewer_launch::AppError>(prepared);
    std::cerr << tizen_tool_viewer_launch::RenderError(error, command.format)
              << '\n';
    return error.exit_code;
  }

  const auto launched = tizen_tool_viewer_launch::LaunchPayload(
      command,
      std::get<tizen_tool_viewer_launch::PersistedPayload>(prepared));
  if (std::holds_alternative<tizen_tool_viewer_launch::AppError>(launched)) {
    const auto& error = std::get<tizen_tool_viewer_launch::AppError>(launched);
    std::cerr << tizen_tool_viewer_launch::RenderError(error, command.format)
              << '\n';
    return error.exit_code;
  }

  std::cout << tizen_tool_viewer_launch::RenderReport(
                   std::get<tizen_tool_viewer_launch::LaunchReport>(launched),
                   command.format)
            << '\n';
  return 0;
}
