// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_TYPE_ASSOCIATIVE_H_
#define CAORS_META_DETAIL_BUILTIN_TYPE_ASSOCIATIVE_H_

#include "caors/meta/detail/builtin_type_capability.h"
#include "caors/meta/detail/container_iterators.h"
#include "caors/traits/container.h"

#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> struct associative_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using key_type = typename traits::associative_traits<type>::key_type;
  using mapped_type = typename traits::associative_traits<type>::mapped_type;

  inline static const associative_ops value = [] {
    associative_ops result{
        descriptor_type_id<key_type>(),
        descriptor_type_id<mapped_type>(),
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return traits::associative_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, associative_raw_visitor visitor, void* user_data) -> bool {
          if (storage == nullptr || visitor == nullptr) {
            return false;
          }

          bool keep_going = true;
          traits::associative_traits<type>::for_each(
              *static_cast<const type*>(storage),
              [&](const key_type& key, const mapped_type& mapped) {
                if (!keep_going) {
                  return;
                }

                keep_going = visitor(&key, &mapped, user_data);
              });

          return keep_going;
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            traits::associative_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, const void* key, const void* mapped) -> bool {
          if (storage == nullptr || key == nullptr || mapped == nullptr) {
            return false;
          }

          return traits::associative_traits<type>::insert(*static_cast<type*>(storage),
                                                          *static_cast<const key_type*>(key),
                                                          *static_cast<const mapped_type*>(mapped));
        },
    };

    if constexpr (has_const_container_iterator_v<type>) {
      using iterator_type = decltype(std::declval<const type&>().begin());
      result.begin = &begin_container_iterator<type>;
      result.end = &end_container_iterator<type>;
      result.iterator_clone = &clone_container_iterator<iterator_type>;
      result.iterator_destroy = &destroy_container_iterator<iterator_type>;
      result.iterator_advance = &advance_container_iterator<iterator_type>;
      result.iterator_equal = &equal_container_iterator<iterator_type>;
      result.iterator_key = &associative_iterator_key<iterator_type>;
      result.iterator_value = &associative_iterator_value<iterator_type>;
    }

    result.key_descriptor = descriptor_ptr_of<key_type>();
    result.value_descriptor = descriptor_ptr_of<mapped_type>();
    return result;
  }();
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
