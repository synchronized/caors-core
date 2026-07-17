// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_TEMPLATES_H_

#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/meta/registry.h"

#include <optional>
#include <type_traits>

namespace caors::meta {

template <typename T>
[[nodiscard]] error registry::register_type_for(const type_registration_data& data) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type_id expected_id = detail::descriptor_type_id<value_type>();
  if ((data.id() && data.id() != expected_id) || !expected_id) {
    return {error_domain::meta, errc::invalid_type};
  }

  type_registration_data stored_type = data;
  if (!stored_type.id()) {
    stored_type.set_id(expected_id);
  }

  const error status = register_type(stored_type);
  if (status && status.value() != errc::duplicate_id) {
    return status;
  }

  const error type_info_status = register_cpp_type_info(typeid(value_type), expected_id);
  if (type_info_status) {
    return type_info_status;
  }

  const error descriptor_status = detail::ensure_type_descriptor_for<value_type>();
  if (descriptor_status) {
    return descriptor_status;
  }

  return status;
}

template <typename T> [[nodiscard]] type registry::find_type() const {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type_id id = detail::descriptor_type_id<value_type>();
  if (!id) {
    return {};
  }

  (void)detail::ensure_type_descriptor_for<value_type>();
  return find_type(id);
}

template <typename Child, typename Parent> [[nodiscard]] error registry::set_type_parent_for() {
  using child_type = std::remove_cv_t<std::remove_reference_t<Child>>;
  using parent_type = std::remove_cv_t<std::remove_reference_t<Parent>>;
  static_assert(std::is_base_of_v<parent_type, child_type>, "Parent must be a base class of Child");

  return set_type_parent(
      detail::descriptor_type_id<child_type>(), detail::descriptor_type_id<parent_type>(),
      detail::parent_cast_ops{
          detail::descriptor_type_id<parent_type>(),
          [](const void* value) noexcept -> const void* {
            return value != nullptr
                       ? static_cast<const parent_type*>(static_cast<const child_type*>(value))
                       : nullptr;
          },
          [](void* value) noexcept -> void* {
            return value != nullptr ? static_cast<parent_type*>(static_cast<child_type*>(value))
                                    : nullptr;
          },
      });
}

template <typename... Components>
template <typename Component>
[[nodiscard]] std::optional<Component> view<Components...>::find(type_id id) const {
  if (registry_ == nullptr) {
    return std::nullopt;
  }

  return registry_->template find_component<Component>(id);
}

template <typename... Components>
[[nodiscard]] view<Components...> registry::component_view() const {
  static_assert(sizeof...(Components) > 0, "Meta view requires at least one component type");

  detail::meta_component_key keys[] = {
      detail::meta_component_key_of<std::decay_t<Components>>()...};
  return ::caors::meta::view<Components...>{this, view_component_ids(keys, sizeof...(Components))};
}

} // namespace caors::meta

#endif
