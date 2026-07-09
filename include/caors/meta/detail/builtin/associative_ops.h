// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_ASSOCIATIVE_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_ASSOCIATIVE_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/meta/detail/container_iterators.h"
#include "caors/traits/container.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

using associative_raw_visitor = bool (*)(const void*, const void*, void*);

using associative_size_fn = std::size_t (*)(const void*) noexcept;
using associative_visit_fn = bool (*)(const void*, associative_raw_visitor, void*);
using associative_clear_fn = void (*)(void*) noexcept;
using associative_insert_fn = bool (*)(void*, const void*, const void*);
using associative_iterator_key_fn = const void* (*)(const void*) noexcept;
using associative_iterator_value_fn = const void* (*)(const void*) noexcept;

/** @brief 关联容器运行期访问操作表。 */
struct associative_ops {
  type_id key_type{};
  type_id value_type{};
  associative_size_fn size = nullptr;
  associative_visit_fn visit = nullptr;
  associative_clear_fn clear = nullptr;
  associative_insert_fn insert = nullptr;
  container_iterator_create_fn begin = nullptr;
  container_iterator_create_fn end = nullptr;
  container_iterator_clone_fn iterator_clone = nullptr;
  container_iterator_destroy_fn iterator_destroy = nullptr;
  container_iterator_advance_fn iterator_advance = nullptr;
  container_iterator_equal_fn iterator_equal = nullptr;
  associative_iterator_key_fn iterator_key = nullptr;
  associative_iterator_value_fn iterator_value = nullptr;
  const type_descriptor* key_descriptor = nullptr;
  const type_descriptor* value_descriptor = nullptr;

  [[nodiscard]] bool valid() const noexcept {
    return key_type && value_type && size != nullptr && visit != nullptr;
  }
};

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
