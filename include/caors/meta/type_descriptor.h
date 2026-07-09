// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_DESCRIPTOR_H_
#define CAORS_META_TYPE_DESCRIPTOR_H_

#include "caors/meta/detail/builtin/associative_ops.h"
#include "caors/meta/detail/builtin/enum_ops.h"
#include "caors/meta/detail/builtin/indirect_ops.h"
#include "caors/meta/detail/builtin/pair_ops.h"
#include "caors/meta/detail/builtin/sequence_ops.h"
#include "caors/meta/detail/builtin/set_ops.h"
#include "caors/meta/detail/builtin/tuple_ops.h"
#include "caors/meta/detail/builtin/variant_ops.h"
#include "caors/meta/type.h"

#include <cstddef>
#include <type_traits>
#include <variant>

namespace caors::meta {

/** @brief C++ 类型布局信息。 */
struct type_layout {
  std::size_t size = 0;
  std::size_t alignment = 0;

  [[nodiscard]] bool valid() const noexcept { return size > 0 && alignment > 0; }
};

/** @brief C++ 值的类型擦除操作函数表。 */
struct value_ops {
  detail::value_default_construct_fn default_construct = nullptr;
  detail::value_clone_fn clone = nullptr;
  detail::value_destroy_fn destroy = nullptr;
  detail::value_assign_fn assign = nullptr;
  detail::value_resource_construct_fn resource_construct = nullptr;
  detail::value_resource_destroy_fn resource_destroy = nullptr;
};

/** @brief C++ 值的编译期特征摘要。 */
struct value_traits {
  bool trivially_copyable = false;
  bool trivially_destructible = false;
  bool trivially_default_constructible = false;
};

/** @brief C++ 类型的运行期语义摘要。 */
struct type_semantics {
  type_kind kind = type_kind::value;
  wrapper_semantics wrapper = wrapper_semantics::none;
  bool indirect = false;
  bool nullable = false;
};

/**
 * @brief C++ 类型描述符，保存布局、值操作、类型语义和固定能力入口。
 *
 * `type_descriptor` 是不要求显式注册即可通过 C++ 类型生成的类型事实源。`meta::registry`
 * 只负责保存注册后的名称、GUID、继承和模块组件索引。
 */
struct type_descriptor {
  type_id type{};
  string_name name{};
  type_layout layout{};
  value_ops ops{};
  value_traits traits{};
  type_semantics semantics{};
  detail::builtin_type_capability builtin_capability{};
  const capability_entry* static_capabilities = nullptr;
  std::size_t static_capability_count = 0;

  [[nodiscard]] bool valid() const noexcept { return type && layout.valid(); }
};

template <typename T> inline constexpr bool descriptor_always_false_v = false;

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor& descriptor) noexcept {
  if constexpr (std::is_same_v<Capability, detail::enum_ops> ||
                std::is_same_v<Capability, detail::indirect_ops> ||
                std::is_same_v<Capability, detail::sequence_ops> ||
                std::is_same_v<Capability, detail::set_ops> ||
                std::is_same_v<Capability, detail::associative_ops> ||
                std::is_same_v<Capability, detail::pair_ops> ||
                std::is_same_v<Capability, detail::tuple_ops> ||
                std::is_same_v<Capability, detail::variant_ops>) {
    const auto* value = std::get_if<const Capability*>(&descriptor.builtin_capability);
    return value != nullptr ? *value : nullptr;
  } else {
    static_assert(descriptor_always_false_v<Capability>,
                  "Unsupported CAORS type capability");
  }
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor_capability<Capability>(*descriptor) : nullptr;
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_static_capability(const type_descriptor& descriptor) noexcept {
  const capability_key key = capability_key_of<Capability>();
  for (std::size_t index = 0; index < descriptor.static_capability_count; ++index) {
    const capability_entry& entry = descriptor.static_capabilities[index];
    if (entry.key == key && entry.data != nullptr) {
      return static_cast<const Capability*>(entry.data);
    }
  }

  return nullptr;
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_static_capability(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor_static_capability<Capability>(*descriptor) : nullptr;
}

[[nodiscard]] inline type_kind descriptor_kind(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.kind;
}

[[nodiscard]] inline wrapper_semantics
descriptor_wrapper_semantics(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.wrapper;
}

[[nodiscard]] inline bool descriptor_indirect(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.indirect;
}

[[nodiscard]] inline bool descriptor_nullable(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.nullable;
}

[[nodiscard]] inline type_id descriptor_value_type(const type_descriptor& descriptor) noexcept {
  switch (descriptor_kind(descriptor)) {
  case type_kind::value:
  case type_kind::enumeration:
    return descriptor.type;
  case type_kind::raw_pointer:
  case type_kind::wrapper: {
    const auto* indirect = descriptor_capability<detail::indirect_ops>(descriptor);
    return indirect != nullptr && indirect->value_descriptor != nullptr
               ? indirect->value_descriptor->type
               : type_id{};
  }
  case type_kind::sequence: {
    const auto* sequence = descriptor_capability<detail::sequence_ops>(descriptor);
    return sequence != nullptr && sequence->value_descriptor != nullptr
               ? sequence->value_descriptor->type
               : type_id{};
  }
  case type_kind::set: {
    const auto* set = descriptor_capability<detail::set_ops>(descriptor);
    return set != nullptr && set->value_descriptor != nullptr ? set->value_descriptor->type
                                                              : type_id{};
  }
  case type_kind::associative: {
    const auto* associative = descriptor_capability<detail::associative_ops>(descriptor);
    return associative != nullptr && associative->value_descriptor != nullptr
               ? associative->value_descriptor->type
               : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<detail::pair_ops>(descriptor);
    return pair != nullptr && pair->value_descriptor != nullptr ? pair->value_descriptor->type
                                                                : type_id{};
  }
  case type_kind::tuple:
  case type_kind::variant:
    return {};
  }

  return {};
}

[[nodiscard]] inline type_id descriptor_key_type(const type_descriptor& descriptor) noexcept {
  switch (descriptor_kind(descriptor)) {
  case type_kind::associative: {
    const auto* associative = descriptor_capability<detail::associative_ops>(descriptor);
    return associative != nullptr && associative->key_descriptor != nullptr
               ? associative->key_descriptor->type
               : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<detail::pair_ops>(descriptor);
    return pair != nullptr && pair->key_descriptor != nullptr ? pair->key_descriptor->type
                                                              : type_id{};
  }
  case type_kind::value:
  case type_kind::enumeration:
  case type_kind::raw_pointer:
  case type_kind::wrapper:
  case type_kind::sequence:
  case type_kind::set:
  case type_kind::tuple:
  case type_kind::variant:
    return {};
  }

  return {};
}

template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return detail::type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

} // namespace caors::meta

#endif
