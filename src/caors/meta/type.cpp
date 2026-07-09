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

} // namespace caors::meta
