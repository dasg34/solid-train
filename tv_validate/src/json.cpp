#include "tv_validate/json.hpp"

#if __has_include(<json.h>)
#include <json.h>
#elif __has_include(<json-c/json.h>)
#include <json-c/json.h>
#else
#error "json-c headers are required to build tv_validate."
#endif

#include <string>
#include <utility>

namespace tv_validate {

namespace {

json_object* CloneOrNull(json_object* object) {
  if (object == nullptr) {
    return nullptr;
  }
  return json_object_get(object);
}

json_object* TakeOrNull(JsonValue value) {
  if (value.get() == nullptr) {
    return json_object_new_null();
  }
  return value.release();
}

}  // namespace

JsonValue::JsonValue(json_object* object) noexcept : object_(object) {}

JsonValue::JsonValue(const JsonValue& other) noexcept
    : object_(CloneOrNull(other.object_)) {}

JsonValue& JsonValue::operator=(const JsonValue& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (object_ != nullptr) {
    json_object_put(object_);
  }
  object_ = CloneOrNull(other.object_);
  return *this;
}

JsonValue::JsonValue(JsonValue&& other) noexcept : object_(other.object_) {
  other.object_ = nullptr;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (object_ != nullptr) {
    json_object_put(object_);
  }
  object_ = other.object_;
  other.object_ = nullptr;
  return *this;
}

JsonValue::~JsonValue() {
  if (object_ != nullptr) {
    json_object_put(object_);
  }
}

JsonValue JsonValue::Object() {
  return JsonValue(json_object_new_object());
}

JsonValue JsonValue::Array() {
  return JsonValue(json_object_new_array());
}

JsonValue JsonValue::String(std::string_view value) {
  return JsonValue(
      json_object_new_string_len(value.data(), static_cast<int>(value.size())));
}

JsonValue JsonValue::Double(double value) {
  return JsonValue(json_object_new_double(value));
}

JsonValue JsonValue::Integer(std::int64_t value) {
  return JsonValue(json_object_new_int64(value));
}

JsonValue JsonValue::Boolean(bool value) {
  return JsonValue(json_object_new_boolean(value ? 1 : 0));
}

JsonValue JsonValue::Null() {
  return JsonValue(json_object_new_null());
}

std::variant<JsonValue, AppError> JsonValue::Parse(std::string_view text,
                                                   std::string error_code,
                                                   std::string error_message,
                                                   int exit_code) {
  json_tokener* tokener = json_tokener_new();
  if (tokener == nullptr) {
    return AppError{
        .code = std::move(error_code),
        .message = std::move(error_message),
        .hint = "Failed to allocate json-c tokener.",
        .exit_code = exit_code,
    };
  }

  json_object* parsed = json_tokener_parse_ex(
      tokener, text.data(), static_cast<int>(text.size()));
  const auto error = json_tokener_get_error(tokener);
  if (error != json_tokener_success) {
    const std::string hint = json_tokener_error_desc(error);
    json_tokener_free(tokener);
    if (parsed != nullptr) {
      json_object_put(parsed);
    }
    return AppError{
        .code = std::move(error_code),
        .message = std::move(error_message),
        .hint = hint,
        .exit_code = exit_code,
    };
  }

  json_tokener_free(tokener);
  return JsonValue(parsed);
}

json_object* JsonValue::get() const noexcept {
  return object_;
}

json_object* JsonValue::release() noexcept {
  json_object* object = object_;
  object_ = nullptr;
  return object;
}

bool JsonValue::IsArray() const noexcept {
  return object_ != nullptr && json_object_is_type(object_, json_type_array);
}

bool JsonValue::IsObject() const noexcept {
  return object_ != nullptr && json_object_is_type(object_, json_type_object);
}

bool JsonValue::IsNull() const noexcept {
  return object_ == nullptr || json_object_is_type(object_, json_type_null);
}

std::size_t JsonValue::Size() const noexcept {
  if (IsArray()) {
    return static_cast<std::size_t>(json_object_array_length(object_));
  }
  if (IsObject()) {
    return static_cast<std::size_t>(json_object_object_length(object_));
  }
  return 0;
}

bool JsonValue::AsBoolean(bool fallback) const noexcept {
  if (object_ == nullptr) {
    return fallback;
  }
  return json_object_get_boolean(object_) != 0;
}

double JsonValue::AsDouble(double fallback) const noexcept {
  if (object_ == nullptr || json_object_is_type(object_, json_type_null)) {
    return fallback;
  }
  return json_object_get_double(object_);
}

int JsonValue::AsInt(int fallback) const noexcept {
  if (object_ == nullptr || json_object_is_type(object_, json_type_null)) {
    return fallback;
  }
  return json_object_get_int(object_);
}

std::string JsonValue::AsString(std::string_view fallback) const {
  if (object_ == nullptr || json_object_is_type(object_, json_type_null)) {
    return std::string(fallback);
  }
  if (json_object_is_type(object_, json_type_string)) {
    return json_object_get_string(object_);
  }
  return json_object_to_json_string_ext(object_, JSON_C_TO_STRING_PLAIN);
}

std::string JsonValue::Dump(bool pretty) const {
  if (object_ == nullptr) {
    return "null";
  }
  const int flags =
      pretty ? (JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED)
             : JSON_C_TO_STRING_PLAIN;
  return json_object_to_json_string_ext(object_, flags);
}

JsonValue JsonValue::At(std::string_view key) const {
  if (!IsObject()) {
    return JsonValue();
  }

  json_object* child = nullptr;
  const std::string key_string(key);
  if (!json_object_object_get_ex(object_, key_string.c_str(), &child) ||
      child == nullptr) {
    return JsonValue();
  }

  return JsonValue(json_object_get(child));
}

JsonValue JsonValue::At(std::size_t index) const {
  if (!IsArray()) {
    return JsonValue();
  }
  if (index >= static_cast<std::size_t>(json_object_array_length(object_))) {
    return JsonValue();
  }
  json_object* child =
      json_object_array_get_idx(object_, static_cast<int>(index));
  if (child == nullptr) {
    return JsonValue();
  }
  return JsonValue(json_object_get(child));
}

void ObjectSet(JsonValue& object, std::string_view key, JsonValue value) {
  if (!object.IsObject()) {
    return;
  }
  const std::string key_string(key);
  json_object_object_add(object.get(), key_string.c_str(),
                         TakeOrNull(std::move(value)));
}

void ArrayAppend(JsonValue& array, JsonValue value) {
  if (!array.IsArray()) {
    return;
  }
  json_object_array_add(array.get(), TakeOrNull(std::move(value)));
}

bool ObjectHasKey(const JsonValue& object, std::string_view key) {
  if (!object.IsObject()) {
    return false;
  }
  json_object* child = nullptr;
  const std::string key_string(key);
  return json_object_object_get_ex(object.get(), key_string.c_str(), &child) &&
         child != nullptr;
}

std::vector<std::string> ObjectKeys(const JsonValue& object) {
  std::vector<std::string> keys;
  if (!object.IsObject()) {
    return keys;
  }
  json_object_object_foreach(object.get(), key, val) {
    (void)val;
    keys.emplace_back(key);
  }
  return keys;
}

}  // namespace tv_validate
