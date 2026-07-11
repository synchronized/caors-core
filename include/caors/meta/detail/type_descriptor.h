// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/meta/type.h"
#include "caors/meta/type_ops.h"

#include <cstddef>
#include <type_traits>
#include <variant>

namespace caors::meta::detail {

/** @brief C++ 类型的运行期语义摘要。 */
struct type_semantics {
  type_kind kind = type_kind::value;
  wrapper_semantics wrapper = wrapper_semantics::none;
  bool indirect = false;
  bool nullable = false;
};

/**
 * @brief 内部 C++ 类型描述符，保存布局、值操作、类型语义和固定能力入口。
 *
 * descriptor 是不要求显式注册即可通过 C++ 类型生成的事实源。公开代码应通过
 * `meta::type` 查询这些能力，不直接依赖该内部结构。
 */
struct type_descriptor {
  type_id type{};
  string_name name{};
  value_storage_ops value{};
  type_semantics semantics{};
  builtin_type_capability builtin_capability{};
  const capability_entry* static_capabilities = nullptr;
  std::size_t static_capability_count = 0;

  [[nodiscard]] bool valid() const noexcept { return type && value.valid(); }
};

template <typename T> inline constexpr bool descriptor_always_false_v = false;

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor& descriptor) noexcept {
  if constexpr (std::is_same_v<Capability, value_storage_ops>) {
    return descriptor.value.valid() ? &descriptor.value : nullptr;
  } else if constexpr (std::is_same_v<Capability, enum_ops> ||
                       std::is_same_v<Capability, indirect_ops> ||
                       std::is_same_v<Capability, sequence_ops> ||
                       std::is_same_v<Capability, set_ops> ||
                       std::is_same_v<Capability, associative_ops> ||
                       std::is_same_v<Capability, pair_ops> ||
                       std::is_same_v<Capability, tuple_ops> ||
                       std::is_same_v<Capability, variant_ops>) {
    const auto* value = std::get_if<const Capability*>(&descriptor.builtin_capability);
    return value != nullptr ? *value : nullptr;
  } else {
    static_assert(descriptor_always_false_v<Capability>, "Unsupported CAORS type capability");
  }
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor_capability<Capability>(*descriptor) : nullptr;
}

/** @brief 按 capability key 查询 descriptor 内建能力。 */
[[nodiscard]] inline const void* descriptor_builtin_capability(const type_descriptor& descriptor,
                                                               capability_key key) noexcept {
  if (key == capability_key_of<value_storage_ops>()) {
    return descriptor.value.valid() ? &descriptor.value : nullptr;
  }
  if (key == capability_key_of<enum_ops>()) {
    return descriptor_capability<enum_ops>(descriptor);
  }
  if (key == capability_key_of<indirect_ops>()) {
    return descriptor_capability<indirect_ops>(descriptor);
  }
  if (key == capability_key_of<sequence_ops>()) {
    return descriptor_capability<sequence_ops>(descriptor);
  }
  if (key == capability_key_of<set_ops>()) {
    return descriptor_capability<set_ops>(descriptor);
  }
  if (key == capability_key_of<associative_ops>()) {
    return descriptor_capability<associative_ops>(descriptor);
  }
  if (key == capability_key_of<pair_ops>()) {
    return descriptor_capability<pair_ops>(descriptor);
  }
  if (key == capability_key_of<tuple_ops>()) {
    return descriptor_capability<tuple_ops>(descriptor);
  }
  if (key == capability_key_of<variant_ops>()) {
    return descriptor_capability<variant_ops>(descriptor);
  }

  return nullptr;
}

/** @brief 按 capability key 查询 descriptor 内建能力，空指针返回空。 */
[[nodiscard]] inline const void* descriptor_builtin_capability(const type_descriptor* descriptor,
                                                               capability_key key) noexcept {
  return descriptor != nullptr ? descriptor_builtin_capability(*descriptor, key) : nullptr;
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

/** @brief 从 descriptor 指针取得类型 id，空指针返回无效 id。 */
[[nodiscard]] inline type_id descriptor_id(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor->type : type_id{};
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
    const auto* indirect = descriptor_capability<indirect_ops>(descriptor);
    const meta::type value = indirect != nullptr && indirect->get_value_type != nullptr
                                 ? indirect->get_value_type()
                                 : meta::type{};
    return value ? value.id() : type_id{};
  }
  case type_kind::sequence: {
    const auto* sequence = descriptor_capability<sequence_ops>(descriptor);
    const meta::type value = sequence != nullptr && sequence->get_value_type != nullptr
                                 ? sequence->get_value_type()
                                 : meta::type{};
    return value ? value.id() : type_id{};
  }
  case type_kind::set: {
    const auto* set = descriptor_capability<set_ops>(descriptor);
    const meta::type value =
        set != nullptr && set->get_value_type != nullptr ? set->get_value_type() : meta::type{};
    return value ? value.id() : type_id{};
  }
  case type_kind::associative: {
    const auto* associative = descriptor_capability<associative_ops>(descriptor);
    const meta::type value = associative != nullptr && associative->get_value_type != nullptr
                                 ? associative->get_value_type()
                                 : meta::type{};
    return value ? value.id() : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_ops>(descriptor);
    const meta::type value =
        pair != nullptr && pair->get_value_type != nullptr ? pair->get_value_type() : meta::type{};
    return value ? value.id() : type_id{};
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
    const auto* associative = descriptor_capability<associative_ops>(descriptor);
    const meta::type key = associative != nullptr && associative->get_key_type != nullptr
                               ? associative->get_key_type()
                               : meta::type{};
    return key ? key.id() : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_ops>(descriptor);
    const meta::type key =
        pair != nullptr && pair->get_key_type != nullptr ? pair->get_key_type() : meta::type{};
    return key ? key.id() : type_id{};
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
  return type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;
[[nodiscard]] CAORS_API error ensure_type_descriptor_registered(const type_descriptor& descriptor);

} // namespace caors::meta::detail

#endif
