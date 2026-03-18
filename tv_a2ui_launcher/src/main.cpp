#include <iostream>

#include "tv_a2ui_launcher/cli.hpp"
#include "tv_a2ui_launcher/launcher.hpp"

int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string_view arg1(argv[1]);
    if (arg1 == "--help" || arg1 == "-h") {
      std::cout << tv_a2ui_launcher::RenderHelp() << '\n';
      return 0;
    }
  }

  const auto parsed = tv_a2ui_launcher::ParseCommand(argc, argv);
  if (std::holds_alternative<tv_a2ui_launcher::AppError>(parsed)) {
    const auto& error = std::get<tv_a2ui_launcher::AppError>(parsed);
    if (error.code == "invalid_arguments" && error.message == "Help requested.") {
      std::cout << error.hint << '\n';
      return 0;
    }
    std::cerr << tv_a2ui_launcher::RenderError(
                     error, tv_a2ui_launcher::OutputFormat::kJson)
              << '\n';
    return error.exit_code;
  }

  const auto& command = std::get<tv_a2ui_launcher::LaunchCommand>(parsed);
  const auto prepared = tv_a2ui_launcher::PreparePayload(command, std::cin);
  if (std::holds_alternative<tv_a2ui_launcher::AppError>(prepared)) {
    const auto& error = std::get<tv_a2ui_launcher::AppError>(prepared);
    std::cerr << tv_a2ui_launcher::RenderError(error, command.format) << '\n';
    return error.exit_code;
  }

  const auto launched = tv_a2ui_launcher::LaunchPayload(
      command, std::get<tv_a2ui_launcher::PersistedPayload>(prepared));
  if (std::holds_alternative<tv_a2ui_launcher::AppError>(launched)) {
    const auto& error = std::get<tv_a2ui_launcher::AppError>(launched);
    std::cerr << tv_a2ui_launcher::RenderError(error, command.format) << '\n';
    return error.exit_code;
  }

  std::cout << tv_a2ui_launcher::RenderReport(
                   std::get<tv_a2ui_launcher::LaunchReport>(launched),
                   command.format)
            << '\n';
  return 0;
}
