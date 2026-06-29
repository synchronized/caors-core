// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace caors {
namespace {

[[nodiscard]] error to_serialization_error(meta_component_result result) noexcept {
  switch (result) {
  case meta_component_result::success:
    return {};
  case meta_component_result::invalid_type:
    return {error_domain::serialization, errc::invalid_type};
  case meta_component_result::missing_type:
    return {error_domain::serialization, errc::missing_type};
  case meta_component_result::duplicate_component:
    return {error_domain::serialization, errc::duplicate_component};
  }

  return {error_domain::serialization, errc::invalid_argument};
}

void append_escaped_string(string& output, std::string_view value) {
  output.push_back('"');
  for (const char character : value) {
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      output.push_back(character);
      break;
    }
  }
  output.push_back('"');
}

void append_text(string& output, const std::string& value) {
  output.append(value.data(), value.size());
}

template <typename T>
[[nodiscard]] bool try_write_i64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_i64(static_cast<std::int64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_u64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_u64(static_cast<std::uint64_t>(*stored)).succeeded();
}

template <typename T>
[[nodiscard]] bool try_write_f64(serialization_writer& writer, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  return writer.write_f64(static_cast<double>(*stored)).succeeded();
}

[[nodiscard]] bool try_write_serialized_value(serialization_writer& writer, const value& data) {
  if (const bool* stored = data.try_get<bool>()) {
    return writer.write_bool(*stored).succeeded();
  }

  if (const char* stored = data.try_get<char>()) {
    return writer.write_string(std::string_view{stored, 1}).succeeded();
  }

  if (try_write_i64<signed char>(writer, data) || try_write_i64<short>(writer, data) ||
      try_write_i64<int>(writer, data) || try_write_i64<long>(writer, data) ||
      try_write_i64<long long>(writer, data) || try_write_u64<unsigned char>(writer, data) ||
      try_write_u64<unsigned short>(writer, data) || try_write_u64<unsigned int>(writer, data) ||
      try_write_u64<unsigned long>(writer, data) ||
      try_write_u64<unsigned long long>(writer, data) || try_write_f64<float>(writer, data) ||
      try_write_f64<double>(writer, data) || try_write_f64<long double>(writer, data)) {
    return true;
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return writer.write_string(*stored).succeeded();
  }

  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return writer.write_string(*stored).succeeded();
  }

  return false;
}

[[nodiscard]] serialization_value_result make_serialized_value(const value& data) {
  if (const bool* stored = data.try_get<bool>()) {
    return {{}, serialization_value::from(*stored)};
  }

  if (const char* stored = data.try_get<char>()) {
    return {{}, serialization_value::from(std::string_view{stored, 1})};
  }

  if (const signed char* stored = data.try_get<signed char>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const short* stored = data.try_get<short>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const int* stored = data.try_get<int>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long* stored = data.try_get<long>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }
  if (const long long* stored = data.try_get<long long>()) {
    return {{}, serialization_value::from(static_cast<std::int64_t>(*stored))};
  }

  if (const unsigned char* stored = data.try_get<unsigned char>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned short* stored = data.try_get<unsigned short>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned int* stored = data.try_get<unsigned int>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long* stored = data.try_get<unsigned long>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }
  if (const unsigned long long* stored = data.try_get<unsigned long long>()) {
    return {{}, serialization_value::from(static_cast<std::uint64_t>(*stored))};
  }

  if (const float* stored = data.try_get<float>()) {
    return {{}, serialization_value::from(static_cast<double>(*stored))};
  }
  if (const double* stored = data.try_get<double>()) {
    return {{}, serialization_value::from(*stored)};
  }
  if (const long double* stored = data.try_get<long double>()) {
    return {{}, serialization_value::from(static_cast<double>(*stored))};
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    return {{}, serialization_value::from(*stored)};
  }
  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    return {{}, serialization_value::from(*stored)};
  }

  return {{error_domain::serialization, errc::unsupported}, {}};
}

[[nodiscard]] string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

[[nodiscard]] error validate_field_for_writer(const serialization_field_data& field,
                                              serialization_format_features features) {
  if (has_feature(features, serialization_format_features::named_fields) &&
      serialized_field_name(field).empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (has_feature(features, serialization_format_features::numeric_fields) && field.field_id == 0) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

[[nodiscard]] error validate_field_for_reader(const serialization_field_data& field,
                                              serialization_format_features features) {
  const bool can_use_name = has_feature(features, serialization_format_features::named_fields) &&
                            !serialized_field_name(field).empty();
  const bool can_use_id =
      has_feature(features, serialization_format_features::numeric_fields) && field.field_id != 0;

  if (!can_use_name && !can_use_id) {
    return {error_domain::serialization, errc::invalid_field};
  }

  return {};
}

[[nodiscard]] error write_field(serialization_writer& writer, const reflection_field& field,
                                const void* object, const serialization_field_data& field_data) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.kind() != field_kind::value) {
    return {error_domain::serialization, errc::unsupported};
  }

  value field_value = field.get_value(object);
  if (!field_value.valid()) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = validate_field_for_writer(field_data, writer.features());
  if (status.failed()) {
    return status;
  }

  status = writer.begin_field(field_data);
  if (status.failed()) {
    return status;
  }

  if (!try_write_serialized_value(writer, field_value)) {
    return {error_domain::serialization, errc::unsupported};
  }

  return writer.end_field();
}

template <typename T>
[[nodiscard]] bool try_read_signed_field(serialization_reader& reader,
                                         const reflection_field& field, void* object) {
  std::int64_t value = 0;
  if (reader.read_i64(value).succeeded()) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).failed() ||
      unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return field.try_set<T>(object, static_cast<T>(unsigned_value));
}

template <typename T>
[[nodiscard]] bool try_read_unsigned_field(serialization_reader& reader,
                                           const reflection_field& field, void* object) {
  std::uint64_t value = 0;
  if (reader.read_u64(value).succeeded()) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
      return false;
    }

    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).failed() || signed_value < 0 ||
      static_cast<std::uint64_t>(signed_value) >
          static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    return false;
  }

  return field.try_set<T>(object, static_cast<T>(signed_value));
}

template <typename T>
[[nodiscard]] bool try_read_float_field(serialization_reader& reader, const reflection_field& field,
                                        void* object) {
  double value = 0.0;
  if (reader.read_f64(value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(value));
  }

  std::int64_t signed_value = 0;
  if (reader.read_i64(signed_value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(signed_value));
  }

  std::uint64_t unsigned_value = 0;
  if (reader.read_u64(unsigned_value).succeeded()) {
    return field.try_set<T>(object, static_cast<T>(unsigned_value));
  }

  return false;
}

[[nodiscard]] error read_field(serialization_reader& reader, const reflection_field& field,
                               void* object, const serialization_field_data& field_data) {
  if (!field.valid()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (!field.writable()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  if (field.kind() != field_kind::value) {
    return {error_domain::serialization, errc::unsupported};
  }

  error status = validate_field_for_reader(field_data, reader.features());
  if (status.failed()) {
    return status;
  }

  status = reader.begin_field(field_data);
  if (status.failed()) {
    return status;
  }

  bool bool_value = false;
  bool written =
      reader.read_bool(bool_value).succeeded() && field.try_set<bool>(object, bool_value);

  if (!written) {
    written = try_read_signed_field<signed char>(reader, field, object) ||
              try_read_signed_field<short>(reader, field, object) ||
              try_read_signed_field<int>(reader, field, object) ||
              try_read_signed_field<long>(reader, field, object) ||
              try_read_signed_field<long long>(reader, field, object);
  }

  if (!written) {
    written = try_read_unsigned_field<unsigned char>(reader, field, object) ||
              try_read_unsigned_field<unsigned short>(reader, field, object) ||
              try_read_unsigned_field<unsigned int>(reader, field, object) ||
              try_read_unsigned_field<unsigned long>(reader, field, object) ||
              try_read_unsigned_field<unsigned long long>(reader, field, object);
  }

  if (!written) {
    written = try_read_float_field<float>(reader, field, object) ||
              try_read_float_field<double>(reader, field, object) ||
              try_read_float_field<long double>(reader, field, object);
  }

  if (!written) {
    string string_value;
    if (reader.read_string(string_value).succeeded()) {
      const std::string standard_string{string_value.data(), string_value.size()};
      written = field.try_set<std::string>(object, standard_string);
    }
  }

  if (!written) {
    return {error_domain::serialization, errc::unsupported};
  }

  return reader.end_field();
}

} // namespace

serialization_writer::~serialization_writer() = default;

serialization_reader::~serialization_reader() = default;

serialization_format_features text_serialization_writer::features() const noexcept {
  return serialization_format_features::named_fields |
         serialization_format_features::ordered_fields |
         serialization_format_features::self_describing;
}

error text_serialization_writer::begin_object(const serialization_type_data&) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  text_.push_back('{');
  contexts_.push_back(context{context_kind::object, true});
  mark_value_written();
  return {};
}

error text_serialization_writer::begin_field(const serialization_field_data& field) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const string_name name = serialized_field_name(field);
  if (name.empty()) {
    return {error_domain::serialization, errc::invalid_field};
  }

  context& current = contexts_.back();
  if (!current.first) {
    text_.push_back(',');
  }

  append_escaped_string(text_, name.view());
  text_.push_back(':');
  current.first = false;
  expecting_value_ = true;
  return {};
}

error text_serialization_writer::begin_array(std::size_t) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  text_.push_back('[');
  contexts_.push_back(context{context_kind::array, true});
  mark_value_written();
  return {};
}

error text_serialization_writer::begin_element(std::size_t) {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context& current = contexts_.back();
  if (!current.first) {
    text_.push_back(',');
  }

  current.first = false;
  expecting_value_ = true;
  return {};
}

error text_serialization_writer::write_bool(bool value) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  text_ += value ? "true" : "false";
  mark_value_written();
  return {};
}

error text_serialization_writer::write_i64(std::int64_t value) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  append_text(text_, std::to_string(value));
  mark_value_written();
  return {};
}

error text_serialization_writer::write_u64(std::uint64_t value) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  append_text(text_, std::to_string(value));
  mark_value_written();
  return {};
}

error text_serialization_writer::write_f64(double value) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  std::ostringstream output;
  output << value;
  append_text(text_, output.str());
  mark_value_written();
  return {};
}

error text_serialization_writer::write_string(std::string_view value) {
  error status = prepare_value();
  if (status.failed()) {
    return status;
  }

  append_escaped_string(text_, value);
  mark_value_written();
  return {};
}

error text_serialization_writer::end_element() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error text_serialization_writer::end_array() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::array || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  text_.push_back(']');
  contexts_.pop_back();
  return {};
}

error text_serialization_writer::end_field() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

error text_serialization_writer::end_object() {
  if (contexts_.empty() || contexts_.back().kind != context_kind::object || expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  text_.push_back('}');
  contexts_.pop_back();
  return {};
}

void text_serialization_writer::reset() {
  text_.clear();
  contexts_.clear();
  expecting_value_ = false;
  root_written_ = false;
}

error text_serialization_writer::prepare_value() {
  if (contexts_.empty()) {
    if (root_written_) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    return {};
  }

  if (!expecting_value_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return {};
}

void text_serialization_writer::mark_value_written() noexcept {
  if (contexts_.empty()) {
    root_written_ = true;
    return;
  }

  expecting_value_ = false;
}

serialization_value::serialization_value() = default;

serialization_value::~serialization_value() = default;

serialization_value::serialization_value(const serialization_value& other) = default;

serialization_value::serialization_value(serialization_value&& other) noexcept = default;

serialization_value& serialization_value::operator=(const serialization_value& other) = default;

serialization_value& serialization_value::operator=(serialization_value&& other) noexcept = default;

serialization_value serialization_value::null() { return {}; }

serialization_value serialization_value::from(bool value) {
  serialization_value result;
  result.kind_ = serialization_value_kind::boolean;
  result.bool_value_ = value;
  return result;
}

serialization_value serialization_value::from(std::int64_t value) {
  serialization_value result;
  result.kind_ = serialization_value_kind::signed_integer;
  result.i64_value_ = value;
  return result;
}

serialization_value serialization_value::from(std::uint64_t value) {
  serialization_value result;
  result.kind_ = serialization_value_kind::unsigned_integer;
  result.u64_value_ = value;
  return result;
}

serialization_value serialization_value::from(double value) {
  serialization_value result;
  result.kind_ = serialization_value_kind::floating;
  result.f64_value_ = value;
  return result;
}

serialization_value serialization_value::from(std::string_view value) {
  serialization_value result;
  result.kind_ = serialization_value_kind::string;
  result.string_value_.assign(value.data(), value.size());
  return result;
}

serialization_value serialization_value::array(array_type values) {
  serialization_value result;
  result.kind_ = serialization_value_kind::array;
  result.array_value_ = std::move(values);
  return result;
}

serialization_value serialization_value::object(object_type members) {
  serialization_value result;
  result.kind_ = serialization_value_kind::object;
  result.object_value_ = std::move(members);
  return result;
}

value_serialization_reader::value_serialization_reader(const serialization_value& value) noexcept
    : root_(&value) {}

serialization_format_features value_serialization_reader::features() const noexcept {
  return serialization_format_features::named_fields |
         serialization_format_features::numeric_fields |
         serialization_format_features::self_describing;
}

error value_serialization_reader::begin_object(const serialization_type_data&) {
  if (root_ == nullptr || root_->kind() != serialization_value_kind::object || object_open_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_open_ = true;
  current_member_ = nullptr;
  return {};
}

const serialization_member*
value_serialization_reader::find_member(const serialization_field_data& field) const {
  if (root_ == nullptr || root_->kind() != serialization_value_kind::object) {
    return nullptr;
  }

  const string_name name = serialized_field_name(field);
  for (const serialization_member& member : root_->as_object()) {
    if (field.field_id != 0 && member.field_id == field.field_id) {
      return &member;
    }

    if (!name.empty() && member.name == name) {
      return &member;
    }
  }

  return nullptr;
}

const serialization_value* value_serialization_reader::current_value() const noexcept {
  return current_member_ != nullptr ? &current_member_->value : nullptr;
}

error value_serialization_reader::begin_field(const serialization_field_data& field) {
  if (!object_open_ || current_member_ != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_member_ = find_member(field);
  if (current_member_ == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  return {};
}

error value_serialization_reader::read_bool(bool& value) {
  const serialization_value* current = current_value();
  if (current == nullptr || current->kind() != serialization_value_kind::boolean) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_bool();
  return {};
}

error value_serialization_reader::read_i64(std::int64_t& value) {
  const serialization_value* current = current_value();
  if (current == nullptr || current->kind() != serialization_value_kind::signed_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_i64();
  return {};
}

error value_serialization_reader::read_u64(std::uint64_t& value) {
  const serialization_value* current = current_value();
  if (current == nullptr || current->kind() != serialization_value_kind::unsigned_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_u64();
  return {};
}

error value_serialization_reader::read_f64(double& value) {
  const serialization_value* current = current_value();
  if (current == nullptr || current->kind() != serialization_value_kind::floating) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_f64();
  return {};
}

error value_serialization_reader::read_string(string& value) {
  const serialization_value* current = current_value();
  if (current == nullptr || current->kind() != serialization_value_kind::string) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_string();
  return {};
}

error value_serialization_reader::end_field() {
  if (current_member_ == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_member_ = nullptr;
  return {};
}

error value_serialization_reader::end_object() {
  if (!object_open_ || current_member_ != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_open_ = false;
  return {};
}

serialization_registry& serialization_registry::instance() noexcept {
  static serialization_registry registry;
  return registry;
}

error serialization_registry::register_type(type_id id, serialization_type_data data) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!data.valid()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  return to_serialization_error(get_meta_registry().emplace<serialization_type_data>(id, data));
}

bool serialization_registry::contains(type_id id) const {
  return get_meta_registry().has<serialization_type_data>(id);
}

std::optional<serialization_type_data> serialization_registry::find_type(type_id id) const {
  return get_meta_registry().find<serialization_type_data>(id);
}

std::size_t serialization_registry::size() const noexcept {
  return get_meta_registry().count<serialization_type_data>();
}

serialization_registry& get_serialization_registry() noexcept {
  return serialization_registry::instance();
}

error register_serialization(type_id id, serialization_type_data data) {
  return get_serialization_registry().register_type(id, data);
}

error write(serialization_writer& writer, const serialization_value& value) {
  switch (value.kind()) {
  case serialization_value_kind::null:
    return {error_domain::serialization, errc::unsupported};
  case serialization_value_kind::boolean:
    return writer.write_bool(value.as_bool());
  case serialization_value_kind::signed_integer:
    return writer.write_i64(value.as_i64());
  case serialization_value_kind::unsigned_integer:
    return writer.write_u64(value.as_u64());
  case serialization_value_kind::floating:
    return writer.write_f64(value.as_f64());
  case serialization_value_kind::string:
    return writer.write_string(value.as_string());
  case serialization_value_kind::array: {
    error status = writer.begin_array(value.as_array().size());
    if (status.failed()) {
      return status;
    }

    std::size_t index = 0;
    for (const serialization_value& element : value.as_array()) {
      status = writer.begin_element(index);
      if (status.failed()) {
        return status;
      }

      status = write(writer, element);
      if (status.failed()) {
        return status;
      }

      status = writer.end_element();
      if (status.failed()) {
        return status;
      }

      ++index;
    }

    return writer.end_array();
  }
  case serialization_value_kind::object: {
    serialization_type_data type_data;
    type_data.fields.reserve(value.as_object().size());
    for (const serialization_member& member : value.as_object()) {
      type_data.fields.push_back(serialization_field_data{
          member.name,
          member.name,
          member.field_id,
          0,
          0,
          false,
          false,
      });
    }

    error status = writer.begin_object(type_data);
    if (status.failed()) {
      return status;
    }

    for (std::size_t index = 0; index < value.as_object().size(); ++index) {
      const serialization_member& member = value.as_object()[index];
      status = validate_field_for_writer(type_data.fields[index], writer.features());
      if (status.failed()) {
        return status;
      }

      status = writer.begin_field(type_data.fields[index]);
      if (status.failed()) {
        return status;
      }

      status = write(writer, member.value);
      if (status.failed()) {
        return status;
      }

      status = writer.end_field();
      if (status.failed()) {
        return status;
      }
    }

    return writer.end_object();
  }
  }

  return {error_domain::serialization, errc::invalid_argument};
}

error serialize(const reflection_type& type, const void* object, serialization_writer& writer) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = writer.begin_object(*serialization);
  if (status.failed()) {
    return status;
  }

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = write_field(writer, *field, object, field_data);
      if (status.failed()) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization_field_data field_data = make_serialization_field(field->name().view());
      status = write_field(writer, *field, object, field_data);
      if (status.failed()) {
        return status;
      }
    }
  }

  return writer.end_object();
}

error deserialize(const reflection_type& type, void* object, serialization_reader& reader) {
  if (!type.valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (object == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {error_domain::serialization, errc::invalid_type};
  }

  error status = reader.begin_object(*serialization);
  if (status.failed()) {
    return status;
  }

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::not_found};
      }

      status = read_field(reader, *field, object, field_data);
      if (status.failed()) {
        return status;
      }
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {error_domain::serialization, errc::invalid_field};
      }

      serialization_field_data field_data = make_serialization_field(field->name().view());
      status = read_field(reader, *field, object, field_data);
      if (status.failed()) {
        return status;
      }
    }
  }

  return reader.end_object();
}

serialization_value_result to_serialization_value(const reflection_type& type, const void* object) {
  if (!type.valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (object == nullptr) {
    return {{error_domain::serialization, errc::invalid_argument}, {}};
  }

  if (!get_serialization_registry().contains(type.id())) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<serialization_type_data> serialization =
      get_serialization_registry().find_type(type.id());
  if (!serialization.has_value() || !serialization->valid()) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  serialization_value::object_type members;

  if (!serialization->fields.empty()) {
    members.reserve(serialization->fields.size());
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped) {
        continue;
      }

      const std::optional<reflection_field> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      value field_value = field->get_value(object);
      if (!field_value.valid()) {
        return {{error_domain::serialization, errc::unsupported}, {}};
      }

      serialization_value_result serialized = make_serialized_value(field_value);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(serialization_member{
          serialized_field_name(field_data),
          field_data.field_id,
          std::move(serialized.value),
      });
    }
  } else {
    members.reserve(type.field_count());
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<reflection_field> field = type.field_at(index);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::invalid_field}, {}};
      }

      value field_value = field->get_value(object);
      if (!field_value.valid()) {
        return {{error_domain::serialization, errc::unsupported}, {}};
      }

      serialization_value_result serialized = make_serialized_value(field_value);
      if (serialized.failed()) {
        return serialized;
      }

      members.push_back(serialization_member{
          field->name(),
          0,
          std::move(serialized.value),
      });
    }
  }

  return {{}, serialization_value::object(std::move(members))};
}

error serialize(type_id id, const void* object, serialization_writer& writer) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!get_serialization_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return serialize(*type, object, writer);
}

error deserialize(type_id id, void* object, serialization_reader& reader) {
  if (!id) {
    return {error_domain::serialization, errc::invalid_type};
  }

  if (!get_serialization_registry().contains(id)) {
    return {error_domain::serialization, errc::missing_type};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {error_domain::serialization, errc::not_found};
  }

  return deserialize(*type, object, reader);
}

serialization_value_result to_serialization_value(type_id id, const void* object) {
  if (!id) {
    return {{error_domain::serialization, errc::invalid_type}, {}};
  }

  if (!get_serialization_registry().contains(id)) {
    return {{error_domain::serialization, errc::missing_type}, {}};
  }

  const std::optional<reflection_type> type = find_reflection(id);
  if (!type.has_value()) {
    return {{error_domain::serialization, errc::not_found}, {}};
  }

  return to_serialization_value(*type, object);
}

serialization_text_result serialize_text(const reflection_type& type, const void* object) {
  text_serialization_writer writer;
  const error status = serialize(type, object, writer);
  if (status.failed()) {
    return {status, {}};
  }

  return {status, writer.text()};
}

serialization_text_result serialize_text(type_id id, const void* object) {
  text_serialization_writer writer;
  const error status = serialize(id, object, writer);
  if (status.failed()) {
    return {status, {}};
  }

  return {status, writer.text()};
}

} // namespace caors
