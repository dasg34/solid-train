#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <variant>

#include "tv_presentation_validate/cli.hpp"
#include "tv_presentation_validate/error.hpp"
#include "tv_presentation_validate/json.hpp"
#include "tv_presentation_validate/validator.hpp"

namespace {

using tv_presentation_validate::AppError;
using tv_presentation_validate::Command;
using tv_presentation_validate::DescribeCommand;
using tv_presentation_validate::JsonValue;
using tv_presentation_validate::ObjectSet;
using tv_presentation_validate::OutputFormat;
using tv_presentation_validate::ValidateCommand;

void PrintJson(const JsonValue& document, OutputFormat format) {
  std::cout << document.Dump(format == OutputFormat::kPretty) << '\n';
}

void PrintError(const AppError& error, OutputFormat format) {
  JsonValue document = JsonValue::Object();
  ObjectSet(document, "ok", JsonValue::Boolean(false));

  JsonValue error_object = JsonValue::Object();
  ObjectSet(error_object, "code", JsonValue::String(error.code));
  ObjectSet(error_object, "message", JsonValue::String(error.message));
  if (!error.hint.empty()) {
    ObjectSet(error_object, "hint", JsonValue::String(error.hint));
  }
  ObjectSet(document, "error", std::move(error_object));
  PrintJson(document, format);
}

std::variant<std::string, AppError> ReadFile(const std::string& path) {
  std::ifstream handle(path);
  if (!handle.is_open()) {
    return AppError{
        .code = "file_not_found",
        .message = "Failed to open file.",
        .hint = path,
        .exit_code = 3,
    };
  }
  std::ostringstream stream;
  stream << handle.rdbuf();
  return stream.str();
}

std::string ReadStdin() {
  std::ostringstream stream;
  stream << std::cin.rdbuf();
  return stream.str();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1 || (argc > 1 && std::string_view(argv[1]) == "--help") ||
      (argc > 1 && std::string_view(argv[1]) == "-h")) {
    std::cout << tv_presentation_validate::RenderHelp();
    return 0;
  }

  const auto parsed = tv_presentation_validate::ParseCommand(argc, argv);
  if (std::holds_alternative<AppError>(parsed)) {
    PrintError(std::get<AppError>(parsed), OutputFormat::kJson);
    return std::get<AppError>(parsed).exit_code;
  }

  const auto& command = std::get<Command>(parsed);

  return std::visit(
      [](const auto& typed_command) -> int {
        using CommandType = std::decay_t<decltype(typed_command)>;

        if constexpr (std::is_same_v<CommandType, DescribeCommand>) {
          PrintJson(tv_presentation_validate::BuildDescribeDocument(),
                    typed_command.format);
          return 0;
        } else if constexpr (std::is_same_v<CommandType, ValidateCommand>) {
          std::string content;
          std::string file_name;

          if (typed_command.file.empty()) {
            content = ReadStdin();
            file_name = "<stdin>";
          } else {
            const auto read_result = ReadFile(typed_command.file);
            if (std::holds_alternative<AppError>(read_result)) {
              PrintError(std::get<AppError>(read_result),
                         typed_command.format);
              return std::get<AppError>(read_result).exit_code;
            }
            content = std::get<std::string>(read_result);
            file_name = typed_command.file;
          }

          const auto report =
              tv_presentation_validate::Validate(content, file_name);

          if (typed_command.format == OutputFormat::kJson) {
            PrintJson(tv_presentation_validate::ReportToJson(report),
                      typed_command.format);
          } else {
            std::cout << tv_presentation_validate::RenderPrettyReport(report);
          }
          return report.passed ? 0 : 1;
        }
      },
      command);
}
