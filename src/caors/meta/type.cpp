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

std::size_t type::size() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.size : 0;
}

std::size_t type::alignment() const noexcept {
  return descriptor_ != nullptr ? descriptor_->layout.alignment : 0;
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
  const type_descriptor* value_descriptor = nullptr;
  switch (descriptor_kind(*descriptor_)) {
  case type_kind::value:
  case type_kind::enumeration:
    value_descriptor = descriptor_;
    break;
  case type_kind::raw_pointer:
  case type_kind::wrapper:
    if (const auto* indirect_ops = get_capability<meta::indirect_ops>()) {
      value_descriptor = indirect_ops->value_descriptor;
    }
    break;
  case type_kind::sequence:
    if (const auto* sequence_ops = get_capability<meta::sequence_ops>()) {
      value_descriptor = sequence_ops->value_descriptor;
    }
    break;
  case type_kind::set:
    if (const auto* set_ops = get_capability<meta::set_ops>()) {
      value_descriptor = set_ops->value_descriptor;
    }
    break;
  case type_kind::associative:
    if (const auto* associative_ops = get_capability<meta::associative_ops>()) {
      value_descriptor = associative_ops->value_descriptor;
    }
    break;
  case type_kind::pair:
    if (const auto* pair_ops = get_capability<meta::pair_ops>()) {
      value_descriptor = pair_ops->value_descriptor;
    }
    break;
  case type_kind::tuple:
  case type_kind::variant:
    break;
  }

  return make_type_view(value_type, value_descriptor);
}

type type::get_key_type() const noexcept {
  if (descriptor_ == nullptr) {
    return {};
  }

  type_id key_type{};
  const type_descriptor* key_descriptor = nullptr;
  switch (descriptor_kind(*descriptor_)) {
  case type_kind::associative:
    if (const auto* associative_ops = get_capability<meta::associative_ops>()) {
      key_type = descriptor_key_type(*descriptor_);
      key_descriptor = associative_ops->key_descriptor;
    }
    break;
  case type_kind::pair:
    if (const auto* pair_ops = get_capability<meta::pair_ops>()) {
      key_type = descriptor_key_type(*descriptor_);
      key_descriptor = pair_ops->key_descriptor;
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

  return make_type_view(key_type, key_descriptor);
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

value_default_construct_fn get_default_construct_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.default_construct : nullptr;
}

value_clone_fn get_clone_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.clone : nullptr;
}

value_destroy_fn get_destroy_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.destroy : nullptr;
}

value_assign_fn get_assign_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.assign : nullptr;
}

value_resource_construct_fn get_resource_construct_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.resource_construct : nullptr;
}

value_resource_destroy_fn get_resource_destroy_value_fn(const meta::type& type) noexcept {
  return type.descriptor_ != nullptr ? type.descriptor_->ops.resource_destroy : nullptr;
}

} // namespace caors::meta
