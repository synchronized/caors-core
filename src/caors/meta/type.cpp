// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

namespace caors::meta {

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
  return descriptor_ != nullptr ? descriptor_kind(*descriptor_) : type_kind::value;
}

wrapper_semantics type::wrapper() const noexcept {
  return descriptor_ != nullptr ? descriptor_wrapper_semantics(*descriptor_)
                                : wrapper_semantics::none;
}

bool type::indirect() const noexcept {
  return descriptor_ != nullptr && descriptor_indirect(*descriptor_);
}

bool type::nullable() const noexcept {
  return descriptor_ != nullptr && descriptor_nullable(*descriptor_);
}

type type::get_value_type() const noexcept {
  if (descriptor_ == nullptr) {
    return {};
  }

  const type_id value_type = descriptor_value_type(*descriptor_);
  meta::type value = {};
  switch (descriptor_kind(*descriptor_)) {
  case type_kind::value:
  case type_kind::enumeration:
    value = *this;
    break;
  case type_kind::raw_pointer:
  case type_kind::wrapper:
    if (const auto* indirect_ops = get_capability<meta::indirect_ops>()) {
      value =
          indirect_ops->get_value_type != nullptr ? indirect_ops->get_value_type() : meta::type{};
    }
    break;
  case type_kind::sequence:
    if (const auto* sequence_ops = get_capability<meta::sequence_ops>()) {
      value =
          sequence_ops->get_value_type != nullptr ? sequence_ops->get_value_type() : meta::type{};
    }
    break;
  case type_kind::set:
    if (const auto* set_ops = get_capability<meta::set_ops>()) {
      value = set_ops->get_value_type != nullptr ? set_ops->get_value_type() : meta::type{};
    }
    break;
  case type_kind::associative:
    if (const auto* associative_ops = get_capability<meta::associative_ops>()) {
      value = associative_ops->get_value_type != nullptr ? associative_ops->get_value_type()
                                                         : meta::type{};
    }
    break;
  case type_kind::pair:
    if (const auto* pair_ops = get_capability<meta::pair_ops>()) {
      value = pair_ops->get_value_type != nullptr ? pair_ops->get_value_type() : meta::type{};
    }
    break;
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return value && value.id() == value_type ? value : meta::type{};
}

type type::get_key_type() const noexcept {
  if (descriptor_ == nullptr) {
    return {};
  }

  type_id key_type{};
  meta::type key = {};
  switch (descriptor_kind(*descriptor_)) {
  case type_kind::associative:
    if (const auto* associative_ops = get_capability<meta::associative_ops>()) {
      key_type = descriptor_key_type(*descriptor_);
      key =
          associative_ops->get_key_type != nullptr ? associative_ops->get_key_type() : meta::type{};
    }
    break;
  case type_kind::pair:
    if (const auto* pair_ops = get_capability<meta::pair_ops>()) {
      key_type = descriptor_key_type(*descriptor_);
      key = pair_ops->get_key_type != nullptr ? pair_ops->get_key_type() : meta::type{};
    }
    break;
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

  return key && key.id() == key_type ? key : meta::type{};
}

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

const type_descriptor* descriptor_of(const meta::type& type) noexcept { return type.descriptor_; }

value_default_construct_fn get_default_construct_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.default_construct : nullptr;
}

value_clone_fn get_clone_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.clone : nullptr;
}

value_destroy_fn get_destroy_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.destroy : nullptr;
}

value_assign_fn get_assign_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.assign : nullptr;
}

value_resource_construct_fn get_resource_construct_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.resource_construct : nullptr;
}

value_resource_destroy_fn get_resource_destroy_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->value.ops.resource_destroy : nullptr;
}

} // namespace caors::meta
