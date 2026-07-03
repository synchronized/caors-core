// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/serialization/dom_reader.h"

namespace caors::serialization {
namespace {

[[nodiscard]] core::string_name serialized_field_name(const field_data& data) noexcept {
  return data.serialized_name.empty() ? data.field_name : data.serialized_name;
}

} // namespace

variant_reader::variant_reader(const variant& value) noexcept : root_(&value) {}

format_features variant_reader::features() const noexcept {
  return format_features::named_fields | format_features::numeric_fields |
         format_features::self_describing;
}

error variant_reader::begin_object(const type_data&) {
  const variant* object = object_stack_.empty() ? root_ : current_value();
  if (object == nullptr || object->kind() != variant_kind::object) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_stack_.push_back(object);
  current_members_.push_back(nullptr);
  return {};
}

const variant_member* variant_reader::find_member(const field_data& field) const {
  if (object_stack_.empty()) {
    return nullptr;
  }

  const variant* object = object_stack_.back();
  if (object == nullptr || object->kind() != variant_kind::object) {
    return nullptr;
  }

  const core::string_name name = serialized_field_name(field);
  for (const variant_member& member : object->as_object()) {
    if (field.field_id != 0 && member.field_id == field.field_id) {
      return &member;
    }

    if (!name.empty() && member.name == name) {
      return &member;
    }
  }

  return nullptr;
}

const variant* variant_reader::current_value() const noexcept {
  if (!dictionary_stack_.empty() && dictionary_stack_.back().current != nullptr) {
    return dictionary_stack_.back().current;
  }

  if (!current_members_.empty() && current_members_.back() != nullptr &&
      object_stack_.size() > array_stack_.size()) {
    return &current_members_.back()->value;
  }

  if (!current_elements_.empty() && current_elements_.back() != nullptr) {
    return current_elements_.back();
  }

  if (!current_members_.empty() && current_members_.back() != nullptr) {
    return &current_members_.back()->value;
  }

  return nullptr;
}

error variant_reader::begin_field(const field_data& field) {
  if (object_stack_.empty() || current_members_.empty() || current_members_.back() != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_members_.back() = find_member(field);
  if (current_members_.back() == nullptr) {
    return {error_domain::serialization, errc::not_found};
  }

  return {};
}

error variant_reader::begin_array(std::size_t& size) {
  const variant* array = current_value();
  if (array == nullptr || array->kind() != variant_kind::array) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  array_stack_.push_back(array);
  current_elements_.push_back(nullptr);
  size = array->as_array().size();
  return {};
}

error variant_reader::begin_element(std::size_t index) {
  if (array_stack_.empty() || current_elements_.empty() || current_elements_.back() != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const variant* array = array_stack_.back();
  if (array == nullptr || array->kind() != variant_kind::array ||
      index >= array->as_array().size()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_elements_.back() = &array->as_array()[index];
  return {};
}

error variant_reader::begin_dictionary(std::size_t& size) {
  const variant* dictionary = current_value();
  if (dictionary == nullptr || (dictionary->kind() != variant_kind::dictionary &&
                                dictionary->kind() != variant_kind::array)) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  size = dictionary->kind() == variant_kind::dictionary ? dictionary->as_dictionary().size()
                                                        : dictionary->as_array().size();
  dictionary_stack_.push_back(dictionary_context{dictionary, nullptr, nullptr, nullptr});
  return {};
}

error variant_reader::begin_entry(std::size_t index) {
  if (dictionary_stack_.empty()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_context& context = dictionary_stack_.back();
  if (context.entry != nullptr || context.pair != nullptr || context.current != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  if (context.source->kind() == variant_kind::dictionary) {
    if (index >= context.source->as_dictionary().size()) {
      return {error_domain::serialization, errc::invalid_argument};
    }

    context.entry = &context.source->as_dictionary()[index];
    return {};
  }

  if (index >= context.source->as_array().size()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const variant& pair = context.source->as_array()[index];
  if (pair.kind() != variant_kind::array || pair.as_array().size() != 2) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context.pair = &pair;
  return {};
}

error variant_reader::begin_key() {
  if (dictionary_stack_.empty()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_context& context = dictionary_stack_.back();
  if (context.current != nullptr || (context.entry == nullptr && context.pair == nullptr)) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context.current = context.entry != nullptr ? &context.entry->key : &context.pair->as_array()[0];
  return {};
}

error variant_reader::end_key() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().current == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_stack_.back().current = nullptr;
  return {};
}

error variant_reader::begin_value() {
  if (dictionary_stack_.empty()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_context& context = dictionary_stack_.back();
  if (context.current != nullptr || (context.entry == nullptr && context.pair == nullptr)) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context.current = context.entry != nullptr ? &context.entry->value : &context.pair->as_array()[1];
  return {};
}

error variant_reader::end_value() {
  if (dictionary_stack_.empty() || dictionary_stack_.back().current == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_stack_.back().current = nullptr;
  return {};
}

error variant_reader::end_entry() {
  if (dictionary_stack_.empty()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_context& context = dictionary_stack_.back();
  if (context.current != nullptr || (context.entry == nullptr && context.pair == nullptr)) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  context.entry = nullptr;
  context.pair = nullptr;
  return {};
}

error variant_reader::end_dictionary() {
  if (dictionary_stack_.empty()) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  const dictionary_context& context = dictionary_stack_.back();
  if (context.current != nullptr || context.entry != nullptr || context.pair != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  dictionary_stack_.pop_back();
  return {};
}

error variant_reader::read_bool(bool& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::boolean) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_bool();
  return {};
}

error variant_reader::read_i64(std::int64_t& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::signed_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_i64();
  return {};
}

error variant_reader::read_u64(std::uint64_t& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::unsigned_integer) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_u64();
  return {};
}

error variant_reader::read_f64(double& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::floating) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_f64();
  return {};
}

error variant_reader::read_string(core::string& value) {
  const variant* current = current_value();
  if (current == nullptr || current->kind() != variant_kind::string) {
    return {error_domain::serialization, errc::invalid_type};
  }

  value = current->as_string();
  return {};
}

bool variant_reader::is_null() const noexcept {
  const variant* current = current_value();
  return current != nullptr && current->is_null();
}

error variant_reader::end_element() {
  if (current_elements_.empty() || current_elements_.back() == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_elements_.back() = nullptr;
  return {};
}

error variant_reader::end_array() {
  if (array_stack_.empty() || current_elements_.empty() || current_elements_.back() != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  array_stack_.pop_back();
  current_elements_.pop_back();
  return {};
}

error variant_reader::end_field() {
  if (current_members_.empty() || current_members_.back() == nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  current_members_.back() = nullptr;
  return {};
}

error variant_reader::end_object() {
  if (object_stack_.empty() || current_members_.empty() || current_members_.back() != nullptr) {
    return {error_domain::serialization, errc::invalid_argument};
  }

  object_stack_.pop_back();
  current_members_.pop_back();
  return {};
}

} // namespace caors::serialization
