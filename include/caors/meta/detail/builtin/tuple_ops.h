// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_TUPLE_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_TUPLE_OPS_H_

#include "caors/core/containers.h"
#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using tuple_const_element_fn = const void* (*)(const void*) noexcept;
using tuple_mutable_element_fn = void* (*)(void*) noexcept;

/** @brief tuple-like 单个元素运行期访问操作表。 */
struct tuple_element_ops {
  type_id value_type{};
  tuple_const_element_fn element = nullptr;
  tuple_mutable_element_fn mutable_element = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept { return value_type && element != nullptr; }
};

/** @brief tuple-like 运行期访问操作表。 */
struct tuple_ops {
  vector<tuple_element_ops> elements{};

  [[nodiscard]] bool valid() const noexcept {
    for (const tuple_element_ops& element : elements) {
      if (!element.valid()) {
        return false;
      }
    }

    return true;
  }
};

template <typename Tuple, std::size_t Index> struct tuple_element_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  [[nodiscard]] static const void* element(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::tuple_traits<type>::template element<Index>(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static void* mutable_element(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::tuple_traits<type>::template mutable_element<Index>(
        *static_cast<type*>(storage));
  }

  inline static const tuple_element_ops value{
      descriptor_type_id<element_type>(),
      &element,
      &mutable_element,
      descriptor_ptr_of<element_type>(),
  };
};

template <typename T> struct tuple_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  template <std::size_t... Indices>
  [[nodiscard]] static tuple_ops make(std::index_sequence<Indices...>) {
    tuple_ops data{};
    data.elements.reserve(sizeof...(Indices));
    (data.elements.push_back(tuple_element_ops_model<type, Indices>::value), ...);
    return data;
  }

  inline static const tuple_ops value =
      make(std::make_index_sequence<traits::tuple_traits<type>::size>{});
};

template <typename T> [[nodiscard]] const tuple_ops* tuple_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_ops_model<type>::value.valid() ? &tuple_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
