// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string_view>

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

void write_escaped_string(std::ostream& output, std::string_view value) {
  output << '"';
  for (const char character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << character;
      break;
    }
  }
  output << '"';
}

template <typename T> [[nodiscard]] bool try_write_value(std::ostream& output, const value& data) {
  const T* stored = data.try_get<T>();
  if (stored == nullptr) {
    return false;
  }

  output << *stored;
  return true;
}

[[nodiscard]] bool try_write_serialized_value(std::ostream& output, const value& data) {
  if (const bool* stored = data.try_get<bool>()) {
    output << (*stored ? "true" : "false");
    return true;
  }

  if (const char* stored = data.try_get<char>()) {
    write_escaped_string(output, std::string_view{stored, 1});
    return true;
  }

  if (try_write_value<signed char>(output, data) || try_write_value<unsigned char>(output, data) ||
      try_write_value<short>(output, data) || try_write_value<unsigned short>(output, data) ||
      try_write_value<int>(output, data) || try_write_value<unsigned int>(output, data) ||
      try_write_value<long>(output, data) || try_write_value<unsigned long>(output, data) ||
      try_write_value<long long>(output, data) ||
      try_write_value<unsigned long long>(output, data) || try_write_value<float>(output, data) ||
      try_write_value<double>(output, data) || try_write_value<long double>(output, data)) {
    return true;
  }

  if (const std::string* stored = data.try_get<std::string>()) {
    write_escaped_string(output, *stored);
    return true;
  }

  if (const std::string_view* stored = data.try_get<std::string_view>()) {
    write_escaped_string(output, *stored);
    return true;
  }

  return false;
}

[[nodiscard]] string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

[[nodiscard]] serialization_text_result write_field(std::ostream& output, const field_view& field,
                                                    const void* object, string_name output_name,
                                                    bool write_separator) {
  if (!field.valid()) {
    return {{error_domain::serialization, errc::invalid_field}, {}};
  }

  if (field.kind() != field_kind::value) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  value field_value = field.get_value(object);
  if (!field_value.valid()) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  if (write_separator) {
    output << ',';
  }

  write_escaped_string(output, output_name.view());
  output << ':';

  if (!try_write_serialized_value(output, field_value)) {
    return {{error_domain::serialization, errc::unsupported}, {}};
  }

  return {};
}

} // namespace

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

serialization_text_result serialize_text(const reflection_type& type, const void* object) {
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

  std::ostringstream output;
  output << '{';

  bool wrote_field = false;

  if (!serialization->fields.empty()) {
    for (const serialization_field_data& field_data : serialization->fields) {
      if (field_data.skipped) {
        continue;
      }

      const std::optional<field_view> field = type.find_field(field_data.field_name);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::not_found}, {}};
      }

      serialization_text_result result =
          write_field(output, *field, object, serialized_field_name(field_data), wrote_field);
      if (result.failed()) {
        return result;
      }

      wrote_field = true;
    }
  } else {
    for (std::size_t index = 0; index < type.field_count(); ++index) {
      const std::optional<field_view> field = type.field_at(index);
      if (!field.has_value()) {
        return {{error_domain::serialization, errc::invalid_field}, {}};
      }

      serialization_text_result result =
          write_field(output, *field, object, field->name(), wrote_field);
      if (result.failed()) {
        return result;
      }

      wrote_field = true;
    }
  }

  output << '}';
  return {{}, output.str()};
}

serialization_text_result serialize_text(type_id id, const void* object) {
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

  return serialize_text(*type, object);
}

} // namespace caors
