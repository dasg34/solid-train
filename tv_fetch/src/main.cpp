#include <iostream>
#include <type_traits>
#include <variant>

#include "tv_fetch/cli.hpp"
#include "tv_fetch/error.hpp"
#include "tv_fetch/json.hpp"
#include "tv_fetch/weather/weather_fetcher.hpp"

namespace {

using tv_fetch::AppError;
using tv_fetch::Command;
using tv_fetch::DescribeCommand;
using tv_fetch::JsonValue;
using tv_fetch::ObjectSet;
using tv_fetch::OutputFormat;
using tv_fetch::WeatherCommand;

JsonValue ErrorToJson(const AppError& error) {
  JsonValue document = JsonValue::Object();
  ObjectSet(document, "ok", JsonValue::Boolean(false));

  JsonValue error_object = JsonValue::Object();
  ObjectSet(error_object, "code", JsonValue::String(error.code));
  ObjectSet(error_object, "message", JsonValue::String(error.message));
  ObjectSet(error_object, "hint",
            error.hint.empty() ? JsonValue::Null() : JsonValue::String(error.hint));
  ObjectSet(document, "error", std::move(error_object));
  return document;
}

void PrintJson(const JsonValue& document, OutputFormat format) {
  std::cout << document.Dump(format == OutputFormat::kPretty) << '\n';
}

void PrintError(const AppError& error, OutputFormat format) {
  PrintJson(ErrorToJson(error), format);
}

tv_fetch::JsonResult ExecuteCommand(const Command& command) {
  return std::visit(
      [](const auto& typed_command) -> tv_fetch::JsonResult {
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

  PrintJson(std::get<JsonValue>(executed), format);
  return 0;
}
