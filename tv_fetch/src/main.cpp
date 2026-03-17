#include <iostream>
#include <variant>

#include <nlohmann/json.hpp>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/error.hpp"
#include "tv_fetch/weather/weather_fetcher.hpp"

namespace {

using tv_fetch::AppError;
using tv_fetch::Command;
using tv_fetch::DescribeCommand;
using tv_fetch::OutputFormat;
using tv_fetch::WeatherCommand;

nlohmann::json ErrorToJson(const AppError& error) {
  nlohmann::json document = {
      {"ok", false},
      {"error",
       {{"code", error.code},
        {"message", error.message},
        {"hint", error.hint.empty() ? nlohmann::json(nullptr)
                                    : nlohmann::json(error.hint)}}},
  };
  return document;
}

void PrintJson(const nlohmann::json& document, OutputFormat format) {
  if (format == OutputFormat::kPretty) {
    std::cout << document.dump(2) << '\n';
    return;
  }
  std::cout << document.dump() << '\n';
}

void PrintError(const AppError& error, OutputFormat format) {
  PrintJson(ErrorToJson(error), format);
}

std::variant<nlohmann::json, AppError> ExecuteCommand(const Command& command) {
  return std::visit(
      [](const auto& typed_command) -> std::variant<nlohmann::json, AppError> {
        using CommandType = std::decay_t<decltype(typed_command)>;
        if constexpr (std::is_same_v<CommandType, DescribeCommand>) {
          return tv_fetch::BuildDescribeDocument(typed_command.target);
        } else if constexpr (std::is_same_v<CommandType, WeatherCommand>) {
          return tv_fetch::weather::Execute(typed_command);
        }
      },
      command);
}

OutputFormat ResolveFormat(const Command& command) {
  return std::visit(
      [](const auto& typed_command) { return typed_command.format; }, command);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1 || (argc > 1 && std::string_view(argv[1]) == "--help") ||
      (argc > 1 && std::string_view(argv[1]) == "-h")) {
    std::cout << tv_fetch::RenderHelp();
    return 0;
  }

  const auto parsed = tv_fetch::ParseCommand(argc, argv);
  if (std::holds_alternative<AppError>(parsed)) {
    PrintError(std::get<AppError>(parsed), OutputFormat::kJson);
    return std::get<AppError>(parsed).exit_code;
  }

  const auto& command = std::get<Command>(parsed);
  const OutputFormat format = ResolveFormat(command);
  const auto executed = ExecuteCommand(command);
  if (std::holds_alternative<AppError>(executed)) {
    PrintError(std::get<AppError>(executed), format);
    return std::get<AppError>(executed).exit_code;
  }

  PrintJson(std::get<nlohmann::json>(executed), format);
  return 0;
}
