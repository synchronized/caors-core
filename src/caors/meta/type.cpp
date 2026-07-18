// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/detail/type_data.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/enum_value.h"
#include "caors/meta/metadata.h"
#include "caors/meta/method.h"
#include "caors/meta/property.h"
#include "caors/meta/registry.h"
#include "caors/meta/type.h"

namespace caors::meta {
namespace {

[[nodiscard]] const metadata_map& empty_metadata_map() noexcept {
  static const metadata_map empty{
      typename metadata_map::allocator_type{&memory::library_resource()}};
  return empty;
}

} // namespace

bool type::valid() const noexcept { return descriptor_ != nullptr && descriptor_->valid(); }

bool type::registered() const noexcept { return data_ != nullptr && data_->registered(); }

type_id type::id() const noexcept {
  if (descriptor_ != nullptr) {
    return descriptor_->type;
  }

  return data_ != nullptr ? data_->id() : type_id::invalid();
}

core::string_name type::name() const noexcept {
  if (data_ != nullptr && data_->name()) {
    return data_->name();
  }

  return descriptor_ != nullptr ? descriptor_->name : core::string_name{};
}

core::type_guid type::guid() const noexcept {
  return data_ != nullptr ? data_->guid() : core::type_guid::nil();
}

type_id type::parent_id() const noexcept {
  return data_ != nullptr ? data_->parent_id() : type_id::invalid();
}

type type::parent() const noexcept {
  if (registry_ == nullptr || !parent_id()) {
    return {};
  }

  return registry_->find_type(parent_id());
}

type_kind type::kind() const noexcept {
  return descriptor_ != nullptr ? detail::descriptor_kind(*descriptor_) : type_kind::value;
}

wrapper_semantics type::wrapper() const noexcept {
  return descriptor_ != nullptr ? detail::descriptor_wrapper_semantics(*descriptor_)
                                : wrapper_semantics::none;
}

bool type::indirect() const noexcept {
  return descriptor_ != nullptr && detail::descriptor_indirect(*descriptor_);
}

bool type::nullable() const noexcept {
  return descriptor_ != nullptr && detail::descriptor_nullable(*descriptor_);
}

const metadata_map& type::metadata() const noexcept {
  if (registry_ == nullptr || !id()) {
    return empty_metadata_map();
  }

  const type_metadata* data = registry_->find_component_ptr<type_metadata>(id());
  return data != nullptr ? data->values : empty_metadata_map();
}

const metadata_value* type::find_metadata(metadata_key key) const noexcept {
  return meta::find_metadata(metadata(), key);
}

type type::get_value_type() const noexcept {
  if (descriptor_ == nullptr) {
    return {};
  }

  switch (detail::descriptor_kind(*descriptor_)) {
  case type_kind::value:
  case type_kind::enumeration:
    return *this;
  case type_kind::raw_pointer:
  case type_kind::wrapper: {
    if (const auto* indirect_ops = get_raw_ops<meta::indirect_ops>()) {
      return indirect_ops->get_value_type != nullptr ? indirect_ops->get_value_type()
                                                     : meta::type{};
    }
    break;
  }
  case type_kind::sequence: {
    if (const auto* sequence_ops = get_raw_ops<meta::sequence_ops>()) {
      return sequence_ops->get_value_type != nullptr ? sequence_ops->get_value_type()
                                                     : meta::type{};
    }
    break;
  }
  case type_kind::set: {
    if (const auto* set_ops = get_raw_ops<meta::set_ops>()) {
      return set_ops->get_value_type != nullptr ? set_ops->get_value_type() : meta::type{};
    }
    break;
  }
  case type_kind::associative: {
    if (const auto* associative_ops = get_raw_ops<meta::associative_ops>()) {
      return associative_ops->get_value_type != nullptr ? associative_ops->get_value_type()
                                                        : meta::type{};
    }
    break;
  }
  case type_kind::pair: {
    if (const auto* pair_ops = get_raw_ops<meta::pair_ops>()) {
      return pair_ops->get_value_type != nullptr ? pair_ops->get_value_type() : meta::type{};
    }
    break;
  }
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return {};
}

type type::get_key_type() const noexcept {
  if (descriptor_ == nullptr) {
    return {};
  }

  switch (detail::descriptor_kind(*descriptor_)) {
  case type_kind::associative: {
    if (const auto* associative_ops = get_raw_ops<meta::associative_ops>()) {
      return associative_ops->get_key_type != nullptr ? associative_ops->get_key_type()
                                                      : meta::type{};
    }
    break;
  }
  case type_kind::pair: {
    if (const auto* pair_ops = get_raw_ops<meta::pair_ops>()) {
      return pair_ops->get_key_type != nullptr ? pair_ops->get_key_type() : meta::type{};
    }
    break;
  }
  case type_kind::value:
  case type_kind::enumeration:
  case type_kind::raw_pointer:
  case type_kind::wrapper:
  case type_kind::sequence:
  case type_kind::set:
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return {};
}

std::size_t type::property_count() const noexcept {
  return data_ != nullptr ? data_->property_count() : 0;
}

property type::property_at(std::size_t index) const {
  return data_ != nullptr ? property{data_->property_at(index)} : property{};
}

property type::find_property(core::string_name property_name) const {
  if (!property_name) {
    return {};
  }

  return data_ != nullptr ? property{data_->find_property_ptr(property_name)} : property{};
}

property type::find_property(std::string_view property_name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(property_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_property(*interned_name);
}

std::size_t type::method_count() const noexcept {
  return data_ != nullptr ? data_->method_count() : 0;
}

method_set type::method_at(std::size_t index) const {
  return data_ != nullptr ? method_set{data_->method_at(index)} : method_set{};
}

method_set type::find_method(core::string_name method_name) const {
  if (!method_name) {
    return {};
  }

  return data_ != nullptr ? method_set{data_->find_method_ptr(method_name)} : method_set{};
}

method_set type::find_method(std::string_view method_name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(method_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_method(*interned_name);
}

namespace detail {

type make_type_view(type_id id, const type_descriptor* descriptor) noexcept {
  if (id) {
    meta::type registered = get_registry().find_type(id);
    if (registered) {
      return registered;
    }
  }

  if (descriptor == nullptr || !descriptor->valid()) {
    return {};
  }

  return meta::type{&get_registry(), nullptr, descriptor};
}

std::size_t enum_value_count(const type& source) noexcept {
  return source.data_ != nullptr ? source.data_->enum_value_count() : 0;
}

enum_value_view enum_value_at(const type& source, std::size_t index) {
  return source.data_ != nullptr ? make_enum_value_view(source.data_->enum_value_at(index))
                                 : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, core::string_name enum_value_name) {
  if (!enum_value_name) {
    return {};
  }

  return source.data_ != nullptr
             ? make_enum_value_view(source.data_->find_enum_value_ptr(enum_value_name))
             : enum_value_view{};
}

enum_value_view find_enum_value(const type& source, std::string_view enum_value_name) {
  const std::optional<core::string_name> interned_name = core::string_name::find(enum_value_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_enum_value(source, *interned_name);
}

enum_value_view find_enum_value_by_i64(const type& source, std::int64_t value) noexcept {
  return source.data_ != nullptr ? make_enum_value_view(source.data_->find_enum_value_ptr(value))
                                 : enum_value_view{};
}

enum_value_view find_enum_value_by_u64(const type& source, std::uint64_t value) noexcept {
  return source.data_ != nullptr ? make_enum_value_view(source.data_->find_enum_value_ptr(value))
                                 : enum_value_view{};
}

} // namespace detail

} // namespace caors::meta
