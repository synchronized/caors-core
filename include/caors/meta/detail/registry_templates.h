// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_REGISTRY_TEMPLATES_H_
#define CAORS_META_DETAIL_REGISTRY_TEMPLATES_H_

#include "caors/meta/detail/type_descriptor_model.h"
#include "caors/meta/registry.h"

#include <type_traits>

namespace caors::meta {

template <typename T>
[[nodiscard]] error registry::register_type_for(string_name name, type_guid guid,
                                                type_id parent_id) {
  using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
  const type_id expected_id = detail::descriptor_type_id<value_type>();
  if (!expected_id) {
    return {error_domain::meta, errc::invalid_type};
  }

  const error status = register_type(expected_id, name, guid, parent_id);
  if (status && status.value() != errc::duplicate_id) {
    return status;
  }

  const error type_info_status = register_cpp_type_info(typeid(value_type), expected_id);
  if (type_info_status) {
    return type_info_status;
  }

  const detail::type_descriptor* descriptor = detail::descriptor_ptr_of<value_type>();
  if (descriptor == nullptr || !descriptor->valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  const error descriptor_status = ensure_type_descriptor(*descriptor);
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

  if (const detail::type_descriptor* descriptor = detail::descriptor_ptr_of<value_type>();
      descriptor != nullptr && descriptor->valid()) {
    (void)const_cast<registry*>(this)->ensure_type_descriptor(*descriptor);
  }
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

} // namespace caors::meta

#endif
