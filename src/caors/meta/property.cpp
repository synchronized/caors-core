// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/property_data.h"
#include "caors/meta/instance.h"
#include "caors/meta/metadata.h"
#include "caors/meta/property.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value_ops.h"

namespace caors::meta {
namespace {

[[nodiscard]] const meta::metadata_map& empty_metadata_map() noexcept {
  static const meta::metadata_map empty{
      typename meta::metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

[[nodiscard]] const meta::property_schema& empty_property_schema() noexcept {
  static const meta::property_schema empty{};
  return empty;
}

} // namespace

property::property(const detail::property_data* property) noexcept : property_(property) {}

bool property::valid() const noexcept {
  return property_ != nullptr && property_->valid();
}

core::string_name property::name() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->name() : core::string_name{};
}

meta::type property::get_owner_type() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->get_owner_type() : meta::type{};
}

meta::type property::get_type() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->get_type() : meta::type{};
}

meta::type property::get_value_type() const noexcept { return get_type().get_value_type(); }

meta::type property::get_element_type() const noexcept { return get_value_type(); }

meta::type property::get_key_type() const noexcept { return get_type().get_key_type(); }

meta::type property::get_first_type() const noexcept { return get_key_type(); }

meta::type property::get_mapped_type() const noexcept { return get_value_type(); }

meta::type property::get_second_type() const noexcept { return get_value_type(); }

meta::type_kind property::kind() const noexcept { return get_type().kind(); }

meta::wrapper_semantics property::wrapper_semantics() const noexcept {
  return get_type().wrapper();
}

bool property::indirect() const noexcept { return get_type().indirect(); }

bool property::nullable() const noexcept { return get_type().nullable(); }

bool property::writable() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr && property->writable();
}

const meta::property_schema& property::schema() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->schema() : empty_property_schema();
}

const meta::metadata_map& property::metadata() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->metadata() : empty_metadata_map();
}

const meta::metadata_value* property::find_metadata(meta::metadata_key key) const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->find_metadata(key) : nullptr;
}

std::size_t property::offset() const noexcept {
  const detail::property_data* property = info();
  return property != nullptr ? property->offset() : 0;
}

std::size_t property::size() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_raw_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.size : 0;
}

std::size_t property::alignment() const noexcept {
  const meta::type meta = get_type();
  const meta::value_storage_ops* value_ops = meta.get_raw_ops<meta::value_storage_ops>();
  return value_ops != nullptr ? value_ops->layout.alignment : 0;
}

const detail::property_data* property::info() const noexcept {
  if (!valid()) {
    return nullptr;
  }

  return property_;
}

any property::get_value(const instance& object) const {
  const detail::property_data* property = info();
  if (property == nullptr || !object) {
    return {};
  }

  return property->get_value(object.data());
}

any property::get_value() const {
  const detail::property_data* property = info();
  if (property == nullptr) {
    return {};
  }

  return property->get_value(nullptr);
}

any_ref property::get_ref(const instance& object) const {
  const detail::property_data* property = info();
  if (property == nullptr || !object) {
    return {};
  }

  return object.writable() ? property->get_ref(object.mutable_data())
                           : property->get_ref(object.data());
}

any_ref property::get_ref() const {
  const detail::property_data* property = info();
  if (property == nullptr) {
    return {};
  }

  return property->get_ref(static_cast<const void*>(nullptr));
}

bool property::try_set_value(const instance& object, const any& source) const {
  const detail::property_data* property = info();
  if (property == nullptr || !object.writable()) {
    return false;
  }

  return property->try_set_value(object.mutable_data(), source);
}

bool property::try_set_value(const any& source) const {
  const detail::property_data* property = info();
  if (property == nullptr) {
    return false;
  }

  return property->try_set_value(nullptr, source);
}

bool property::is_sequence() const noexcept { return kind() == meta::type_kind::sequence; }

bool property::is_set() const noexcept { return kind() == meta::type_kind::set; }

bool property::is_associative() const noexcept { return kind() == meta::type_kind::associative; }

bool property::is_pair() const noexcept { return kind() == meta::type_kind::pair; }

bool property::is_tuple() const noexcept { return kind() == meta::type_kind::tuple; }

bool property::is_variant() const noexcept { return kind() == meta::type_kind::variant; }

} // namespace caors::meta
