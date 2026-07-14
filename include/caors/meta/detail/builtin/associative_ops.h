// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_ASSOCIATIVE_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_ASSOCIATIVE_OPS_H_

#include "caors/meta/detail/builtin/container_iterators.h"
#include "caors/meta/detail/builtin/ops.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using meta::associative_ops;

template <typename T> struct associative_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using key_type = typename traits::associative_traits<type>::key_type;
  using mapped_type = typename traits::associative_traits<type>::mapped_type;

  [[nodiscard]] static meta::type get_key_type() noexcept {
    return make_type_view(descriptor_type_id<key_type>(), descriptor_ptr_of<key_type>());
  }

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return make_type_view(descriptor_type_id<mapped_type>(), descriptor_ptr_of<mapped_type>());
  }

  [[nodiscard]] static std::size_t size(const void* storage) noexcept {
    if (storage == nullptr) {
      return 0;
    }

    return traits::associative_traits<type>::size(*static_cast<const type*>(storage));
  }

  static void clear(void* storage) noexcept {
    if (storage != nullptr) {
      traits::associative_traits<type>::clear(*static_cast<type*>(storage));
    }
  }

  static bool insert(void* storage, const void* key, const void* mapped) {
    if (storage == nullptr || key == nullptr || mapped == nullptr) {
      return false;
    }

    return traits::associative_traits<type>::insert(*static_cast<type*>(storage),
                                                    *static_cast<const key_type*>(key),
                                                    *static_cast<const mapped_type*>(mapped));
  }

  static void fill_iterator_ops(associative_ops& result) noexcept {
    using traits_type = traits::associative_traits<type>;
    if constexpr (has_traits_const_iterator_v<traits_type, type>) {
      const auto* iterator = traits_container_iterator_ops_ptr_of<type, traits_type>();
      result.iterator = *iterator;
      using iterator_type = typename traits_type::const_iterator;
      result.iterator_key = &associative_traits_iterator_key<type, iterator_type>;
      result.iterator_value = &associative_traits_iterator_value<type, iterator_type>;
    } else if constexpr (has_const_container_iterator_v<type>) {
      if (const auto* iterator = container_iterator_ops_ptr_of<type>(); iterator != nullptr) {
        result.iterator = *iterator;
        using iterator_type = decltype(std::declval<const type&>().begin());
        result.iterator_key = &associative_iterator_key<iterator_type>;
        result.iterator_value = &associative_iterator_value<iterator_type>;
      }
    }
  }

  [[nodiscard]] static associative_ops make() {
    associative_ops result{
        &size,
        &clear,
        &insert,
    };

    if constexpr (has_traits_const_iterator_v<traits::associative_traits<type>, type> ||
                  has_const_container_iterator_v<type>) {
      fill_iterator_ops(result);
    }

    result.get_key_type = &get_key_type;
    result.get_value_type = &get_value_type;
    return result;
  }

  inline static const associative_ops value = make();
};

template <typename T> [[nodiscard]] const associative_ops* associative_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::associative_traits<type>::is_associative) {
    return associative_ops_model<type>::value.valid() ? &associative_ops_model<type>::value
                                                      : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
