// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/value/variant.h"

#include <utility>

namespace caors::value {

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

variant variant::dictionary(dictionary_type entries) {
  variant result;
  result.kind_ = variant_kind::dictionary;
  result.dictionary_value_ = std::move(entries);
  return result;
}

variant variant::object(object_type members) {
  variant result;
  result.kind_ = variant_kind::object;
  result.object_value_ = std::move(members);
  return result;
}

bool variant::push_back(variant value) {
  if (!is_array()) {
    return false;
  }

  array_value_.push_back(std::move(value));
  return true;
}

bool variant::add_entry(variant key, variant value) {
  if (!is_dictionary()) {
    return false;
  }

  dictionary_value_.push_back(variant_entry{std::move(key), std::move(value)});
  return true;
}

bool variant::add_member(string_name name, std::uint32_t field_id, variant value) {
  if (!is_object()) {
    return false;
  }

  object_value_.push_back(variant_member{name, field_id, std::move(value)});
  return true;
}

const variant_member* variant::find_member(string_name name) const noexcept {
  if (!is_object() || name.empty()) {
    return nullptr;
  }

  for (const variant_member& member : object_value_) {
    if (member.name == name) {
      return &member;
    }
  }

  return nullptr;
}

const variant_member* variant::find_member(std::uint32_t field_id) const noexcept {
  if (!is_object() || field_id == 0) {
    return nullptr;
  }

  for (const variant_member& member : object_value_) {
    if (member.field_id == field_id) {
      return &member;
    }
  }

  return nullptr;
}

variant_member* variant::find_member(string_name name) noexcept {
  return const_cast<variant_member*>(std::as_const(*this).find_member(name));
}

variant_member* variant::find_member(std::uint32_t field_id) noexcept {
  return const_cast<variant_member*>(std::as_const(*this).find_member(field_id));
}

} // namespace caors::value
