// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_ASSOCIATIVE_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_ASSOCIATIVE_H_

#include "caors/meta/detail/container_registration_iterators.h"
#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename T> [[nodiscard]] error register_associative_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;

    const type_id key = semantic_type_id<key_type>();
    const type_id value = semantic_type_id<mapped_type>();
    if (!id || !key || !value) {
      return {};
    }

    meta::detail::associative_data data{
        key,
        value,
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
      data.begin = &begin_container_iterator<type>;
      data.end = &end_container_iterator<type>;
      data.iterator_clone = &clone_container_iterator<iterator_type>;
      data.iterator_destroy = &destroy_container_iterator<iterator_type>;
      data.iterator_advance = &advance_container_iterator<iterator_type>;
      data.iterator_equal = &equal_container_iterator<iterator_type>;
      data.iterator_key = &associative_iterator_key<iterator_type>;
      data.iterator_value = &associative_iterator_value<iterator_type>;
    }

    const error result =
        caors::meta::get_registry().emplace<meta::detail::associative_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
