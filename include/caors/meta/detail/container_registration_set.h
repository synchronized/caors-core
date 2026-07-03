// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_CONTAINER_REGISTRATION_SET_H_
#define CAORS_META_DETAIL_CONTAINER_REGISTRATION_SET_H_

#include "caors/meta/detail/container_registration_iterators.h"
#include "caors/meta/registry.h"

namespace caors::meta::detail {
template <typename T> [[nodiscard]] error register_set_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta::detail::set_data data{
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return traits::set_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, set_raw_visitor visitor, void* user_data) -> bool {
          if (storage == nullptr || visitor == nullptr) {
            return false;
          }

          bool keep_going = true;
          traits::set_traits<type>::for_each(*static_cast<const type*>(storage),
                                             [&](const element_type& value) {
                                               if (!keep_going) {
                                                 return;
                                               }

                                               keep_going = visitor(&value, user_data);
                                             });

          return keep_going;
        },
        [](void* storage) noexcept {
          if (storage != nullptr) {
            traits::set_traits<type>::clear(*static_cast<type*>(storage));
          }
        },
        [](void* storage, const void* element) -> bool {
          if (storage == nullptr || element == nullptr) {
            return false;
          }

          return traits::set_traits<type>::insert(*static_cast<type*>(storage),
                                                  *static_cast<const element_type*>(element));
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
      data.iterator_value = &set_iterator_value<iterator_type>;
    }

    const error result = caors::meta::get_registry().emplace<meta::detail::set_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

} // namespace caors::meta::detail

#endif
