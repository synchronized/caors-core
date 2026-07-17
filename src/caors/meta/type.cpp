// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include "caors/meta/detail/type_data.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/field.h"
#include "caors/meta/method.h"

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

std::size_t type::field_count() const noexcept { return data_ != nullptr ? data_->field_count() : 0; }

field type::field_at(std::size_t index) const {
  return data_ != nullptr ? field{data_->field_at(index)} : field{};
}

field type::find_field(core::string_name field_name) const {
  if (!field_name) {
    return {};
  }

  return data_ != nullptr ? field{data_->find_field_ptr(field_name)} : field{};
}

field type::find_field(std::string_view field_name) const {
  const std::optional<core::string_name> interned_name = core::string_name::find(field_name);
  if (!interned_name.has_value()) {
    return {};
  }

  return find_field(*interned_name);
}

std::size_t type::method_count() const noexcept { return data_ != nullptr ? data_->method_count() : 0; }

method type::method_at(std::size_t index) const {
  return data_ != nullptr ? method{data_->method_at(index)} : method{};
}

method type::find_method(core::string_name method_name) const {
  if (!method_name) {
    return {};
  }

  return data_ != nullptr ? method{data_->find_method_ptr(method_name)} : method{};
}

method type::find_method(std::string_view method_name) const {
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

} // namespace detail

} // namespace caors::meta
