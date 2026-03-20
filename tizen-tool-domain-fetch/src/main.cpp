#include <iostream>
#include <type_traits>
#include <variant>

#include "tizen_tool_domain_fetch/cli.hpp"
#include "tizen_tool_domain_fetch/commute/commute_fetcher.hpp"
#include "tizen_tool_domain_fetch/daily/daily_fetcher.hpp"
#include "tizen_tool_domain_fetch/emergency/emergency_fetcher.hpp"
#include "tizen_tool_domain_fetch/error.hpp"
#include "tizen_tool_domain_fetch/finance/finance_fetcher.hpp"
#include "tizen_tool_domain_fetch/json.hpp"
#include "tizen_tool_domain_fetch/news/news_fetcher.hpp"
#include "tizen_tool_domain_fetch/scenario/scenario_fetcher.hpp"
#include "tizen_tool_domain_fetch/schedule/schedule_fetcher.hpp"
#include "tizen_tool_domain_fetch/sports/sports_fetcher.hpp"
#include "tizen_tool_domain_fetch/travel/travel_fetcher.hpp"
#include "tizen_tool_domain_fetch/weather/weather_fetcher.hpp"

namespace {

using tizen_tool_domain_fetch::AppError;
using tizen_tool_domain_fetch::Command;
using tizen_tool_domain_fetch::CommuteCommand;
using tizen_tool_domain_fetch::DescribeCommand;
using tizen_tool_domain_fetch::DailyCommand;
using tizen_tool_domain_fetch::EmergencyCommand;
using tizen_tool_domain_fetch::FinanceCommand;
using tizen_tool_domain_fetch::JsonValue;
using tizen_tool_domain_fetch::NewsCommand;
using tizen_tool_domain_fetch::ObjectSet;
using tizen_tool_domain_fetch::OutputFormat;
using tizen_tool_domain_fetch::ScheduleCommand;
using tizen_tool_domain_fetch::ScenarioCommand;
using tizen_tool_domain_fetch::SportsCommand;
using tizen_tool_domain_fetch::TravelCommand;
using tizen_tool_domain_fetch::WeatherCommand;

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

tizen_tool_domain_fetch::JsonResult ExecuteCommand(const Command& command) {
  return std::visit(
      [](const auto& typed_command) -> tizen_tool_domain_fetch::JsonResult {
        using CommandType = std::decay_t<decltype(typed_command)>;
        if constexpr (std::is_same_v<CommandType, DescribeCommand>) {
          return tizen_tool_domain_fetch::BuildDescribeDocument(typed_command.target);
        } else if constexpr (std::is_same_v<CommandType, WeatherCommand>) {
          return tizen_tool_domain_fetch::weather::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, NewsCommand>) {
          return tizen_tool_domain_fetch::news::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, FinanceCommand>) {
          return tizen_tool_domain_fetch::finance::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, CommuteCommand>) {
          return tizen_tool_domain_fetch::commute::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, SportsCommand>) {
          return tizen_tool_domain_fetch::sports::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, ScheduleCommand>) {
          return tizen_tool_domain_fetch::schedule::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, TravelCommand>) {
          return tizen_tool_domain_fetch::travel::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, EmergencyCommand>) {
          return tizen_tool_domain_fetch::emergency::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, DailyCommand>) {
          return tizen_tool_domain_fetch::daily::Execute(typed_command);
        } else if constexpr (std::is_same_v<CommandType, ScenarioCommand>) {
          return tizen_tool_domain_fetch::scenario::Execute(typed_command);
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
    std::cout << tizen_tool_domain_fetch::RenderHelp();
    return 0;
  }

  const auto parsed = tizen_tool_domain_fetch::ParseCommand(argc, argv);
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
