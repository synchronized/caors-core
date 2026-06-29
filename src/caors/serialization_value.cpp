// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization.h"

#include <string_view>
#include <utility>

namespace caors {
namespace {

[[nodiscard]] string_name serialized_field_name(const serialization_field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

} // namespace

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

} // namespace caors
