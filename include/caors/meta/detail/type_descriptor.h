// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/detail/builtin/ops.h"
#include "caors/meta/type.h"
#include "caors/meta/type_ops.h"
#include "caors/meta/value_ops.h"

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
 * @brief 内部 C++ 类型描述符，保存布局、值操作、类型语义、内置函数表和静态能力入口。
 *
 * descriptor 是不要求显式注册即可通过 C++ 类型生成的事实源。公开代码应通过
 * `meta::type` 查询这些信息，不直接依赖该内部结构。
 */
struct type_descriptor {
  type_id type{};
  string_name name{};
  value_storage_ops value{};
  type_semantics semantics{};
  builtin_ops builtin{};
  const capability_entry* static_capabilities = nullptr;
  std::size_t static_capability_count = 0;

  [[nodiscard]] bool valid() const noexcept { return type && value.valid(); }
};

template <typename T> inline constexpr bool descriptor_always_false_v = false;

template <typename Ops>
[[nodiscard]] inline const Ops* descriptor_ops(const type_descriptor& descriptor) noexcept {
  if constexpr (std::is_same_v<Ops, value_storage_ops>) {
    return descriptor.value.valid() ? &descriptor.value : nullptr;
  } else if constexpr (std::is_same_v<Ops, scalar_ops> || std::is_same_v<Ops, enum_ops> ||
                       std::is_same_v<Ops, indirect_ops> || std::is_same_v<Ops, sequence_ops> ||
                       std::is_same_v<Ops, set_ops> || std::is_same_v<Ops, associative_ops> ||
                       std::is_same_v<Ops, pair_ops> || std::is_same_v<Ops, tuple_ops> ||
                       std::is_same_v<Ops, variant_ops>) {
    const auto* value = std::get_if<const Ops*>(&descriptor.builtin);
    return value != nullptr ? *value : nullptr;
  } else {
    static_assert(descriptor_always_false_v<Ops>, "Unsupported CAORS descriptor ops");
  }
}

/** @brief 按 capability key 查询 descriptor 内建函数表。 */
[[nodiscard]] inline const void* descriptor_builtin_ops(const type_descriptor& descriptor,
                                                        capability_key key) noexcept {
  if (key == capability_key_of<value_storage_ops>()) {
    return descriptor.value.valid() ? &descriptor.value : nullptr;
  }
  if (key == capability_key_of<scalar_ops>()) {
    return descriptor_ops<scalar_ops>(descriptor);
  }
  if (key == capability_key_of<enum_ops>()) {
    return descriptor_ops<enum_ops>(descriptor);
  }
  if (key == capability_key_of<indirect_ops>()) {
    return descriptor_ops<indirect_ops>(descriptor);
  }
  if (key == capability_key_of<sequence_ops>()) {
    return descriptor_ops<sequence_ops>(descriptor);
  }
  if (key == capability_key_of<set_ops>()) {
    return descriptor_ops<set_ops>(descriptor);
  }
  if (key == capability_key_of<associative_ops>()) {
    return descriptor_ops<associative_ops>(descriptor);
  }
  if (key == capability_key_of<pair_ops>()) {
    return descriptor_ops<pair_ops>(descriptor);
  }
  if (key == capability_key_of<tuple_ops>()) {
    return descriptor_ops<tuple_ops>(descriptor);
  }
  if (key == capability_key_of<variant_ops>()) {
    return descriptor_ops<variant_ops>(descriptor);
  }

  return nullptr;
}

/** @brief 按 capability key 查询 descriptor 内建函数表，空指针返回空。 */
[[nodiscard]] inline const void* descriptor_builtin_ops(const type_descriptor* descriptor,
                                                        capability_key key) noexcept {
  return descriptor != nullptr ? descriptor_builtin_ops(*descriptor, key) : nullptr;
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

template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

template <typename T> [[nodiscard]] static const type_descriptor* descriptor_ptr_of() noexcept;
[[nodiscard]] CAORS_API error ensure_type_descriptor_registered(const type_descriptor& descriptor);

} // namespace caors::meta::detail

#endif
