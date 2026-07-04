// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/detail/container.h"
#include "caors/meta/detail/container_iterators.h"
#include "caors/meta/type_semantics.h"
#include "caors/object/ref.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

struct type_descriptor;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;

/** @brief meta 内部类型描述符，保存布局、值操作和运行期视图能力。 */
struct type_descriptor {
  type_id type{};
  std::size_t size = 0;
  std::size_t alignment = 0;
  value_default_construct_fn default_construct = nullptr;
  value_clone_fn clone = nullptr;
  value_destroy_fn destroy = nullptr;
  value_assign_fn assign = nullptr;
  type_kind kind = type_kind::value;
  meta::wrapper_semantics wrapper_semantics = meta::wrapper_semantics::none;
  type_id value_type{};
  type_id key_type{};
  std::uint8_t pointer_depth = 0;
  bool nullable = false;
  const enum_data* enumeration = nullptr;
  const indirect_data* indirect = nullptr;
  const objects::detail::object_ref_data* object_ref = nullptr;
  const sequence_data* sequence = nullptr;
  const set_data* set = nullptr;
  const associative_data* associative = nullptr;
  const pair_data* pair = nullptr;
  const tuple_data* tuple = nullptr;
  const variant_data* variant = nullptr;

  [[nodiscard]] bool valid() const noexcept { return type && size > 0 && alignment > 0; }

  [[nodiscard]] bool has_view_capability() const noexcept {
    return object_ref != nullptr || indirect != nullptr || sequence != nullptr || set != nullptr ||
           associative != nullptr || pair != nullptr || tuple != nullptr || variant != nullptr;
  }
};

template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

template <typename T>
inline constexpr bool descriptor_supported_wrapper_v =
    traits::wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool descriptor_nested_type_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || descriptor_supported_wrapper_v<T>;

template <typename T> void fill_descriptor_semantics(type_descriptor& descriptor) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_cv_t<std::remove_pointer_t<type>>;

    static_assert(!descriptor_nested_type_indirection_v<value_type>,
                  "CAORS value supports at most one pointer or wrapper level");

    descriptor.kind = type_kind::raw_pointer;
    descriptor.wrapper_semantics = meta::wrapper_semantics::inline_value;
    descriptor.value_type = descriptor_type_id<value_type>();
    descriptor.pointer_depth = 1;
    descriptor.nullable = true;
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;

    descriptor.kind = type_kind::sequence;
    descriptor.value_type = descriptor_type_id<element_type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;

    descriptor.kind = type_kind::set;
    descriptor.value_type = descriptor_type_id<element_type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;

    descriptor.kind = type_kind::associative;
    descriptor.key_type = descriptor_type_id<key_type>();
    descriptor.value_type = descriptor_type_id<mapped_type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;

    descriptor.kind = type_kind::pair;
    descriptor.key_type = descriptor_type_id<first_type>();
    descriptor.value_type = descriptor_type_id<second_type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    descriptor.kind = type_kind::tuple;
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    descriptor.kind = type_kind::variant;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    using wrapped_type = typename traits::wrapper_traits<type>::wrapped_type;
    using value_type = std::remove_cv_t<wrapped_type>;

    static_assert(!descriptor_nested_type_indirection_v<value_type>,
                  "CAORS value supports at most one pointer or wrapper level");

    descriptor.kind = type_kind::wrapper;
    descriptor.wrapper_semantics = traits::wrapper_traits<type>::semantics;
    descriptor.value_type = descriptor_type_id<value_type>();
    descriptor.nullable = traits::wrapper_traits<type>::nullable;
  } else {
    if constexpr (std::is_enum_v<type>) {
      descriptor.kind = type_kind::enumeration;
    } else {
      descriptor.kind = type_kind::value;
    }
    descriptor.value_type = descriptor_type_id<type>();
  }
}

template <typename T> [[nodiscard]] const enum_data* enum_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    static const enum_data data = make_enum_data<type>();
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const indirect_data* indirect_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_pointer_t<type>;
    static const indirect_data data = [] {
      indirect_data result = make_indirect_data<type>();
      result.value_descriptor = descriptor_ptr_of<std::remove_cv_t<value_type>>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper &&
                       traits::wrapper_traits<type>::semantics != wrapper_semantics::unsupported) {
    using value_type = std::remove_cv_t<typename traits::wrapper_traits<type>::wrapped_type>;
    static const indirect_data data = [] {
      indirect_data result = make_indirect_data<type>();
      result.value_descriptor = descriptor_ptr_of<value_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T>
[[nodiscard]] const objects::detail::object_ref_data* object_ref_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return objects::detail::object_ref_data_ptr_of<type>();
}

template <typename T> [[nodiscard]] const sequence_data* sequence_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;
    static const sequence_data data = [] {
      sequence_data result{
          descriptor_type_id<element_type>(),
          [](const void* storage) noexcept -> std::size_t {
            if (storage == nullptr) {
              return 0;
            }

            return traits::sequence_traits<type>::size(*static_cast<const type*>(storage));
          },
          [](const void* storage, std::size_t index) noexcept -> const void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::sequence_traits<type>::element(*static_cast<const type*>(storage),
                                                          index);
          },
          [](void* storage, std::size_t index) noexcept -> void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::sequence_traits<type>::mutable_element(*static_cast<type*>(storage),
                                                                  index);
          },
          [](void* storage) noexcept {
            if (storage != nullptr) {
              traits::sequence_traits<type>::clear(*static_cast<type*>(storage));
            }
          },
          [](void* storage, std::size_t size) -> bool {
            if (storage == nullptr) {
              return false;
            }

            return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size);
          },
          [](void* storage, const void* element) -> bool {
            if (storage == nullptr || element == nullptr) {
              return false;
            }

            return traits::sequence_traits<type>::append(
                *static_cast<type*>(storage), *static_cast<const element_type*>(element));
          },
      };

      if constexpr (requires { traits::sequence_traits<type>::read_in_place; }) {
        result.read_in_place = traits::sequence_traits<type>::read_in_place;
      }

      if constexpr (requires { traits::sequence_traits<type>::can_read; }) {
        result.can_read = traits::sequence_traits<type>::can_read;
      }

      result.value_descriptor = descriptor_ptr_of<element_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const set_data* set_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;
    static const set_data data = [] {
      set_data result{
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

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const associative_data* associative_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;
    static const associative_data data = [] {
      associative_data result{
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

            return traits::associative_traits<type>::insert(
                *static_cast<type*>(storage), *static_cast<const key_type*>(key),
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

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const pair_data* pair_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;
    static const pair_data data = [] {
      pair_data result{
          descriptor_type_id<first_type>(),
          descriptor_type_id<second_type>(),
          [](const void* storage) noexcept -> const void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::pair_traits<type>::first(*static_cast<const type*>(storage));
          },
          [](void* storage) noexcept -> void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::pair_traits<type>::mutable_first(*static_cast<type*>(storage));
          },
          [](const void* storage) noexcept -> const void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::pair_traits<type>::second(*static_cast<const type*>(storage));
          },
          [](void* storage) noexcept -> void* {
            if (storage == nullptr) {
              return nullptr;
            }

            return traits::pair_traits<type>::mutable_second(*static_cast<type*>(storage));
          },
      };

      result.key_descriptor = descriptor_ptr_of<first_type>();
      result.value_descriptor = descriptor_ptr_of<second_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Tuple, std::size_t Index>
[[nodiscard]] tuple_element_data make_tuple_element_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  return tuple_element_data{
      descriptor_type_id<element_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::tuple_traits<type>::template element<Index>(
            *static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::tuple_traits<type>::template mutable_element<Index>(
            *static_cast<type*>(storage));
      },
      descriptor_ptr_of<element_type>(),
  };
}

template <typename Tuple, std::size_t... Indices>
[[nodiscard]] tuple_data make_tuple_descriptor_data(std::index_sequence<Indices...>) {
  tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_descriptor_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] const tuple_data* tuple_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    static const tuple_data data = make_tuple_descriptor_data<type>(
        std::make_index_sequence<traits::tuple_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Variant, std::size_t Index>
[[nodiscard]] variant_alternative_data make_variant_alternative_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  return variant_alternative_data{
      descriptor_type_id<alternative_type>(),
      [](const void* storage) noexcept -> const void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::variant_traits<type>::template alternative<Index>(
            *static_cast<const type*>(storage));
      },
      [](void* storage) noexcept -> void* {
        if (storage == nullptr) {
          return nullptr;
        }

        return traits::variant_traits<type>::template mutable_alternative<Index>(
            *static_cast<type*>(storage));
      },
      [](void* storage) -> bool {
        if (storage == nullptr) {
          return false;
        }

        return traits::variant_traits<type>::template emplace<Index>(*static_cast<type*>(storage));
      },
      descriptor_ptr_of<alternative_type>(),
  };
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] variant_data make_variant_descriptor_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  variant_data data{};
  data.alternatives.reserve(sizeof...(Indices));
  (data.alternatives.push_back(make_variant_alternative_descriptor_data<type, Indices>()), ...);
  data.index = [](const void* storage) noexcept -> std::size_t {
    if (storage == nullptr) {
      return static_cast<std::size_t>(-1);
    }

    return traits::variant_traits<type>::index(*static_cast<const type*>(storage));
  };
  return data;
}

template <typename T> [[nodiscard]] const variant_data* variant_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    static const variant_data data = make_variant_descriptor_data<type>(
        std::make_index_sequence<traits::variant_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> struct descriptor_holder {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const type_descriptor value = [] {
    type_descriptor descriptor{};
    descriptor.type = descriptor_type_id<type>();
    descriptor.size = sizeof(type);
    descriptor.alignment = alignof(type);
    descriptor.default_construct = default_construct_fn_of<type>();
    descriptor.clone = value_clone_fn_of<type>();
    descriptor.destroy = value_destroy_fn_of<type>();
    descriptor.assign = value_assign_fn_of<type>();
    fill_descriptor_semantics<type>(descriptor);
    descriptor.enumeration = enum_data_ptr_of<type>();
    descriptor.indirect = indirect_data_ptr_of<type>();
    descriptor.object_ref = object_ref_data_ptr_of<type>();
    descriptor.sequence = sequence_data_ptr_of<type>();
    descriptor.set = set_data_ptr_of<type>();
    descriptor.associative = associative_data_ptr_of<type>();
    descriptor.pair = pair_data_ptr_of<type>();
    descriptor.tuple = tuple_data_ptr_of<type>();
    descriptor.variant = variant_data_ptr_of<type>();
    return descriptor;
  }();
};

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &descriptor_holder<type>::value;
}

template <typename T> [[nodiscard]] error ensure_type_descriptor_for(type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  const type_descriptor* descriptor = descriptor_ptr_of<T>();
  if (descriptor == nullptr || !descriptor->valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  const type_descriptor* existing = get_registry().find_ptr<type_descriptor>(id);
  if (existing != nullptr) {
    if (existing->type != descriptor->type || existing->size != descriptor->size ||
        existing->alignment != descriptor->alignment) {
      return {error_domain::meta, errc::invalid_type};
    }

    return {};
  }

  return ensure_component_for(id, *descriptor);
}

} // namespace caors::meta::detail

#endif
