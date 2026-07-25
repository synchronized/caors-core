// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object/object_factory_capability.h"

#include "caors/meta/detail/type_access.h"

#include "../meta/detail/registry_state.h"

namespace caors::objects {

bool object_factory_capability::constructible() const noexcept {
  return valid() && ops_->constructible();
}

object* object_factory_capability::try_create(memory_resource* resource) const noexcept {
  const meta::detail::layer_call_guard guard{meta::detail::type_access::layer_of(type_)};
  return guard && constructible() && ops_->try_create != nullptr ? ops_->try_create(resource)
                                                                 : nullptr;
}

object* object_factory_capability::create(memory_resource* resource) const {
  const meta::detail::layer_call_guard guard{meta::detail::type_access::layer_of(type_)};
  return guard && constructible() && ops_->create != nullptr ? ops_->create(resource) : nullptr;
}

bool object_factory_capability::destroy(object* instance,
                                        memory_resource* resource) const noexcept {
  const meta::detail::layer_call_guard guard{meta::detail::type_access::layer_of(type_)};
  if (!guard || !valid() || instance == nullptr || ops_->destroy == nullptr) {
    return false;
  }

  ops_->destroy(instance, resource);
  return true;
}

} // namespace caors::objects
