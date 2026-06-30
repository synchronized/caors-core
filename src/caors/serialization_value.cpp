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

variant::variant() = default;

variant::~variant() = default;

variant::variant(const variant& other) = default;

variant::variant(variant&& other) noexcept = default;

variant& variant::operator=(const variant& other) = default;

variant& variant::operator=(variant&& other) noexcept = default;

variant variant::null() { return {}; }

variant variant::from(bool value) {
  variant result;
  result.kind_ = variant_kind::boolean;
  result.bool_value_ = value;
  return result;
}

variant variant::from(std::int64_t value) {
  variant result;
  result.kind_ = variant_kind::signed_integer;
  result.i64_value_ = value;
  return result;
}

variant variant::from(std::uint64_t value) {
  variant result;
  result.kind_ = variant_kind::unsigned_integer;
  result.u64_value_ = value;
  return result;
}

variant variant::from(double value) {
  variant result;
  result.kind_ = variant_kind::floating;
  result.f64_value_ = value;
  return result;
}

variant variant::from(std::string_view value) {
  variant result;
  result.kind_ = variant_kind::string;
  result.string_value_.assign(value.data(), value.size());
  return result;
}

variant variant::array(array_type values) {
  variant result;
  result.kind_ = variant_kind::array;
  result.array_value_ = std::move(values);
  return result;
}

variant variant::object(object_type members) {
  variant result;
  result.kind_ = variant_kind::object;
  result.object_value_ = std::move(members);
  return result;
}

variant_serialization_reader::variant_serialization_reader(const variant& value) noexcept
    : root_(&value) {}

serialization_format_features variant_serialization_reader::features() const noexcept {
  return serialization_format_features::named_fields |
         serialization_format_features::numeric_fields |
         serialization_format_features::self_describing;
}

error variant_serialization_reader::begin_object(const serialization_type_data&) {
  if (root_ == nullptr || root_->kind() != variant_kind::object || object_open_) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_open_ = true;
  current_member_ = nullptr;
  return {};
}

const variant_member*
variant_serialization_reader::find_member(const serialization_field_data& field) const {
  if (root_ == nullptr || root_->kind() != variant_kind::object) {
    return nullptr;
  }

  const string_name name = serialized_field_name(field);
  for (const variant_member& member : root_->as_object()) {
    if (field.field_id != 0 && member.field_id == field.field_id) {
      return &member;
    }

    if (!name.empty() && member.name == name) {
      return &member;
    }
  }

  return nullptr;
}

const variant* variant_serialization_reader::current_value() const noexcept {
  return current_member_ != nullptr ? &current_member_->value : nullptr;
}

error variant_serialization_reader::begin_field(const serialization_field_data& field) {
  if (!object_open_ || current_member_ != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_member_ = find_member(field);
  if (current_member_ == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  return {};
}

error variant_serialization_reader::read_bool(bool& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::boolean) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_bool();
  return {};
}

error variant_serialization_reader::read_i64(std::int64_t& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::signed_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_i64();
  return {};
}

error variant_serialization_reader::read_u64(std::uint64_t& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::unsigned_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_u64();
  return {};
}

error variant_serialization_reader::read_f64(double& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::floating) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_f64();
  return {};
}

error variant_serialization_reader::read_string(string& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::string) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_string();
  return {};
}

error variant_serialization_reader::end_field() {
  if (current_member_ == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_member_ = nullptr;
  return {};
}

error variant_serialization_reader::end_object() {
  if (!object_open_ || current_member_ != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_open_ = false;
  return {};
}

} // namespace caors
