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

using set_raw_visitor = bool (*)(const void*, void*);

using set_size_fn = std::size_t (*)(const void*) noexcept;
using set_visit_fn = bool (*)(const void*, set_raw_visitor, void*);
using set_clear_fn = void (*)(void*) noexcept;
using set_insert_fn = bool (*)(void*, const void*);
using set_iterator_value_fn = const void* (*)(const void*) noexcept;

/** @brief 集合容器运行期访问操作表。 */
struct set_ops {
  type_id value_type{};
  set_size_fn size = nullptr;
  set_visit_fn visit = nullptr;
  set_clear_fn clear = nullptr;
  set_insert_fn insert = nullptr;
  container_iterator_create_fn begin = nullptr;
  container_iterator_create_fn end = nullptr;
  container_iterator_clone_fn iterator_clone = nullptr;
  container_iterator_destroy_fn iterator_destroy = nullptr;
  container_iterator_advance_fn iterator_advance = nullptr;
  container_iterator_equal_fn iterator_equal = nullptr;
  set_iterator_value_fn iterator_value = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return value_type && size != nullptr && visit != nullptr;
  }
};

template <typename T> struct set_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::set_traits<type>::element_type;

  inline static const set_ops value = [] {
    set_ops result{
        descriptor_type_id<element_type>(),
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
                                             [&](const element_type& element) {
                                               if (!keep_going) {
                                                 return;
                                               }

                                               keep_going = visitor(&element, user_data);
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
      result.begin = &begin_container_iterator<type>;
      result.end = &end_container_iterator<type>;
      result.iterator_clone = &clone_container_iterator<iterator_type>;
      result.iterator_destroy = &destroy_container_iterator<iterator_type>;
      result.iterator_advance = &advance_container_iterator<iterator_type>;
      result.iterator_equal = &equal_container_iterator<iterator_type>;
      result.iterator_value = &set_iterator_value<iterator_type>;
    }

    result.value_descriptor = descriptor_ptr_of<element_type>();
    return result;
  }();
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
