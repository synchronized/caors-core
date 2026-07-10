// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_SET_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_SET_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/meta/detail/builtin/container_iterators.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using meta::set_ops;

template <typename T> struct set_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::set_traits<type>::element_type;

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return meta::make_type_view(descriptor_type_id<element_type>(),
                                descriptor_ptr_of<element_type>());
  }

  [[nodiscard]] static std::size_t size(const void* storage) noexcept {
    if (storage == nullptr) {
      return 0;
    }

    return traits::set_traits<type>::size(*static_cast<const type*>(storage));
  }

  static void clear(void* storage) noexcept {
    if (storage != nullptr) {
      traits::set_traits<type>::clear(*static_cast<type*>(storage));
    }
  }

  static bool insert(void* storage, const void* element) {
    if (storage == nullptr || element == nullptr) {
      return false;
    }

    return traits::set_traits<type>::insert(*static_cast<type*>(storage),
                                            *static_cast<const element_type*>(element));
  }

  static void fill_iterator_ops(set_ops& result) noexcept {
    using traits_type = traits::set_traits<type>;
    if constexpr (has_traits_const_iterator_v<traits_type, type>) {
      const auto* iterator = traits_container_iterator_ops_ptr_of<type, traits_type>();
      result.iterator = *iterator;
      using iterator_type = typename traits_type::const_iterator;
      result.iterator_value = &set_traits_iterator_value<type, iterator_type>;
    } else if constexpr (has_const_container_iterator_v<type>) {
      if (const auto* iterator = container_iterator_ops_ptr_of<type>(); iterator != nullptr) {
        result.iterator = *iterator;
        using iterator_type = decltype(std::declval<const type&>().begin());
        result.iterator_value = &set_iterator_value<iterator_type>;
      }
    }
  }

  [[nodiscard]] static set_ops make() {
    set_ops result{
        &size,
        &clear,
        &insert,
    };

    if constexpr (has_traits_const_iterator_v<traits::set_traits<type>, type> ||
                  has_const_container_iterator_v<type>) {
      fill_iterator_ops(result);
    }

    result.get_value_type = &get_value_type;
    return result;
  }

  inline static const set_ops value = make();
};

template <typename T> [[nodiscard]] const set_ops* set_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    return set_ops_model<type>::value.valid() ? &set_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
