#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "tv_presentation_validate/error.hpp"

struct json_object;

namespace tv_presentation_validate {

class JsonValue {
 public:
  JsonValue() = default;
  explicit JsonValue(json_object* object) noexcept;
  JsonValue(const JsonValue& other) noexcept;
  JsonValue& operator=(const JsonValue& other) noexcept;
  JsonValue(JsonValue&& other) noexcept;
  JsonValue& operator=(JsonValue&& other) noexcept;
  ~JsonValue();

  static JsonValue Object();
  static JsonValue Array();
  static JsonValue String(std::string_view value);
  static JsonValue Double(double value);
  static JsonValue Integer(std::int64_t value);
  static JsonValue Boolean(bool value);
  static JsonValue Null();
  static std::variant<JsonValue, AppError> Parse(std::string_view text,
                                                 std::string error_code,
                                                 std::string error_message,
                                                 int exit_code);

  json_object* get() const noexcept;
  json_object* release() noexcept;

  bool IsArray() const noexcept;
  bool IsObject() const noexcept;
  bool IsNull() const noexcept;
  bool IsString() const noexcept;
  bool IsBoolean() const noexcept;
  bool IsNumber() const noexcept;
  std::size_t Size() const noexcept;

  bool AsBoolean(bool fallback = false) const noexcept;
  double AsDouble(double fallback = 0.0) const noexcept;
  int AsInt(int fallback = 0) const noexcept;
  std::string AsString(std::string_view fallback = {}) const;

  std::string Dump(bool pretty) const;

  JsonValue At(std::string_view key) const;
  JsonValue At(std::size_t index) const;

 private:
  json_object* object_ = nullptr;
};

using JsonResult = std::variant<JsonValue, AppError>;

void ObjectSet(JsonValue& object, std::string_view key, JsonValue value);
void ArrayAppend(JsonValue& array, JsonValue value);
bool ObjectHasKey(const JsonValue& object, std::string_view key);
std::vector<std::string> ObjectKeys(const JsonValue& object);

}  // namespace tv_presentation_validate
