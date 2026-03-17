#include "tv_fetch/scenario/scenario_fetcher.hpp"

#include <array>
#include <optional>
#include <string_view>

#include "tv_fetch/support.hpp"

namespace tv_fetch::scenario {

namespace {

struct ScenarioMeta {
  ScenarioCommand::Kind kind;
  std::string_view command_name;
  std::string_view domain;
  std::string_view fixture_file;
  std::string_view description;
};

constexpr std::array<ScenarioMeta, 6> kScenarioMetas = {{
    {ScenarioCommand::Kind::kFamily,
     "family",
     "family",
     "mock_family.json",
     "Return a shared family board payload with reminders and household notices."},
    {ScenarioCommand::Kind::kMealDelivery,
     "meal-delivery",
     "meal-delivery",
     "mock_meal_delivery.json",
     "Return a meal or delivery status payload with ETA-style cards."},
    {ScenarioCommand::Kind::kMedia,
     "media",
     "media",
     "mock_media.json",
     "Return a media companion payload for cast, episode, or soundtrack-style panels."},
    {ScenarioCommand::Kind::kShopping,
     "shopping",
     "shopping",
     "mock_shopping.json",
     "Return a shopping decision support payload for compare-and-decide layouts."},
    {ScenarioCommand::Kind::kSmartHome,
     "smart-home",
     "smart-home",
     "mock_smart_home.json",
     "Return a smart home summary payload for doors, climate, and alerts."},
    {ScenarioCommand::Kind::kWellness,
     "wellness",
     "wellness",
     "mock_wellness.json",
     "Return a wellness card payload for activity, sleep, or stretch reminders."},
}};

const ScenarioMeta* FindMeta(ScenarioCommand::Kind kind) {
  for (const auto& meta : kScenarioMetas) {
    if (meta.kind == kind) {
      return &meta;
    }
  }
  return nullptr;
}

}  // namespace

std::optional<ScenarioCommand::Kind> ParseKind(std::string_view command_name) {
  for (const auto& meta : kScenarioMetas) {
    if (meta.command_name == command_name) {
      return meta.kind;
    }
  }
  return std::nullopt;
}

std::string_view CommandName(ScenarioCommand::Kind kind) {
  const ScenarioMeta* meta = FindMeta(kind);
  return meta == nullptr ? "unknown" : meta->command_name;
}

JsonValue Describe(const ScenarioCommand::Kind kind) {
  const ScenarioMeta* meta = FindMeta(kind);
  if (meta == nullptr) {
    return MakeObject({
        {"name", JsonValue::String("unknown")},
        {"warning", JsonValue::String("Unknown scenario command.")},
    });
  }

  return MakeObject({
      {"name", JsonValue::String(meta->command_name)},
      {"description", JsonValue::String(meta->description)},
      {"supports_live", JsonValue::Boolean(false)},
      {"default_source", JsonValue::String("mock")},
      {"sources", MakeArray({JsonValue::String("mock")})},
      {"parameters",
       MakeArray({
           MakeObject({
               {"name", JsonValue::String("--source")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"values", MakeArray({JsonValue::String("mock")})},
               {"default", JsonValue::String("mock")},
           }),
           MakeObject({
               {"name", JsonValue::String("--dry-run")},
               {"type", JsonValue::String("boolean")},
               {"required", JsonValue::Boolean(false)},
               {"default", JsonValue::Boolean(false)},
           }),
           MakeObject({
               {"name", JsonValue::String("--format")},
               {"type", JsonValue::String("string")},
               {"required", JsonValue::Boolean(false)},
               {"values",
                MakeArray(
                    {JsonValue::String("json"), JsonValue::String("pretty")})},
               {"default", JsonValue::String("json")},
           }),
       })},
      {"output_shape",
       MakeObject({
           {"domain", JsonValue::String(meta->domain)},
           {"source", JsonValue::String("mock")},
           {"title", JsonValue::String("string")},
           {"headline", JsonValue::String("string")},
           {"primaryMetrics", JsonValue::String("array")},
           {"sections", JsonValue::String("array")},
           {"alert", JsonValue::String("object")},
           {"actions", JsonValue::String("array")},
           {"footer", JsonValue::String("string")},
       })},
  });
}

JsonResult Execute(const ScenarioCommand& command) {
  const ScenarioMeta* meta = FindMeta(command.kind);
  if (meta == nullptr) {
    return AppError{
        .code = "invalid_arguments",
        .message = "Unknown scenario command.",
        .hint = "Use tv_fetch describe to inspect supported domains.",
        .exit_code = 2,
    };
  }

  if (command.source != "mock") {
    return AppError{
        .code = "invalid_arguments",
        .message = "This tv_fetch scenario currently supports mock source only.",
        .hint = std::string(meta->command_name) + " --source mock",
        .exit_code = 2,
    };
  }

  if (command.dry_run) {
    const auto path = ResolveFixturePath(meta->fixture_file);
    return MakeObject({
        {"command", JsonValue::String(meta->command_name)},
        {"mode", JsonValue::String("dry-run")},
        {"source", JsonValue::String("mock")},
        {"fixture_path",
         path.empty() ? JsonValue::Null() : JsonValue::String(path.string())},
    });
  }

  return LoadFixturePayload(meta->fixture_file, meta->domain, command.source);
}

}  // namespace tv_fetch::scenario
