// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_VALUE_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/detail/container.h"
#include "caors/meta/detail/container_registration_iterators.h"
#include "caors/meta/type_semantics.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace caors::value::detail {

struct type_descriptor;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;

/** @brief value 层内部类型描述符。 */
struct type_descriptor {
  type_id type{};
  std::size_t size = 0;
  std::size_t alignment = 0;
  meta::detail::value_default_construct_fn default_construct = nullptr;
  meta::detail::value_clone_fn clone = nullptr;
  meta::detail::value_destroy_fn destroy = nullptr;
  meta::detail::value_assign_fn assign = nullptr;
  const meta::type_semantics_data* semantics = nullptr;
  const meta::detail::indirect_data* indirect = nullptr;
  const meta::detail::sequence_data* sequence = nullptr;
  const meta::detail::set_data* set = nullptr;
  const meta::detail::associative_data* associative = nullptr;
  const meta::detail::pair_data* pair = nullptr;
  const meta::detail::tuple_data* tuple = nullptr;
  const meta::detail::variant_data* variant = nullptr;

  [[nodiscard]] bool valid() const noexcept { return type && size > 0 && alignment > 0; }

  [[nodiscard]] bool has_view_capability() const noexcept {
    return indirect != nullptr || sequence != nullptr || set != nullptr || associative != nullptr ||
           pair != nullptr || tuple != nullptr || variant != nullptr;
  }
};

template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return meta::detail::type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

template <typename T>
inline constexpr bool is_supported_wrapper_v =
    traits::wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_type_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

template <typename T> [[nodiscard]] meta::type_semantics_data make_semantics_data() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  meta::type_semantics_data info{};
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;

    info.kind = meta::type_kind::sequence;
    info.value_type = descriptor_type_id<element_type>();
    return info;
  } else if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;

    info.kind = meta::type_kind::set;
    info.value_type = descriptor_type_id<element_type>();
    return info;
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;

    info.kind = meta::type_kind::associative;
    info.key_type = descriptor_type_id<key_type>();
    info.value_type = descriptor_type_id<mapped_type>();
    return info;
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;

    info.kind = meta::type_kind::pair;
    info.key_type = descriptor_type_id<first_type>();
    info.value_type = descriptor_type_id<second_type>();
    return info;
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    info.kind = meta::type_kind::tuple;
    return info;
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    info.kind = meta::type_kind::variant;
    return info;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    using wrapped_type = typename traits::wrapper_traits<type>::wrapped_type;
    using value_type = std::remove_cv_t<wrapped_type>;

    static_assert(!is_nested_type_indirection_v<value_type>,
                  "CAORS value supports at most one pointer or wrapper level");

    info.kind = meta::type_kind::wrapper;
    info.wrapper_semantics = traits::wrapper_traits<type>::semantics;
    info.value_type = descriptor_type_id<value_type>();
    info.nullable = traits::wrapper_traits<type>::nullable;
    return info;
  } else {
    if constexpr (std::is_enum_v<type>) {
      info.kind = meta::type_kind::enumeration;
    } else {
      info.kind = meta::type_kind::value;
    }
    info.value_type = descriptor_type_id<type>();
    return info;
  }
}

template <typename T> struct semantic_data_holder {
  inline static const meta::type_semantics_data value = make_semantics_data<T>();
};

template <typename T> [[nodiscard]] const meta::type_semantics_data* semantics_ptr_of() noexcept {
  return &semantic_data_holder<std::remove_cv_t<std::remove_reference_t<T>>>::value;
}

template <typename T> [[nodiscard]] meta::type_semantics_data make_pointer_semantics_data() {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_type_indirection_v<value_type>,
                "CAORS value supports at most one pointer or wrapper level");

  meta::type_semantics_data info{};
  info.kind = meta::type_kind::raw_pointer;
  info.wrapper_semantics = meta::wrapper_semantics::inline_value;
  info.value_type = descriptor_type_id<value_type>();
  info.pointer_depth = 1;
  info.nullable = true;
  return info;
}

template <typename T> struct semantic_data_holder<T*> {
  inline static const meta::type_semantics_data value = make_pointer_semantics_data<T>();
};

template <typename T> [[nodiscard]] const meta::detail::indirect_data* indirect_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_pointer_t<type>;
    static const meta::detail::indirect_data data = [] {
      meta::detail::indirect_data result = meta::detail::make_indirect_data<type>();
      result.value_descriptor = descriptor_ptr_of<std::remove_cv_t<value_type>>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper &&
                       traits::wrapper_traits<type>::semantics !=
                           meta::wrapper_semantics::unsupported) {
    using value_type = std::remove_cv_t<typename traits::wrapper_traits<type>::wrapped_type>;
    static const meta::detail::indirect_data data = [] {
      meta::detail::indirect_data result = meta::detail::make_indirect_data<type>();
      result.value_descriptor = descriptor_ptr_of<value_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const meta::detail::sequence_data* sequence_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;
    static const meta::detail::sequence_data data = [] {
      meta::detail::sequence_data result{
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

template <typename T> [[nodiscard]] const meta::detail::set_data* set_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;
    static const meta::detail::set_data data = [] {
      meta::detail::set_data result{
          descriptor_type_id<element_type>(),
          [](const void* storage) noexcept -> std::size_t {
            if (storage == nullptr) {
              return 0;
            }

            return traits::set_traits<type>::size(*static_cast<const type*>(storage));
          },
          [](const void* storage, meta::detail::set_raw_visitor visitor, void* user_data) -> bool {
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

      if constexpr (meta::detail::has_const_container_iterator_v<type>) {
        using iterator_type = decltype(std::declval<const type&>().begin());
        result.begin = &meta::detail::begin_container_iterator<type>;
        result.end = &meta::detail::end_container_iterator<type>;
        result.iterator_clone = &meta::detail::clone_container_iterator<iterator_type>;
        result.iterator_destroy = &meta::detail::destroy_container_iterator<iterator_type>;
        result.iterator_advance = &meta::detail::advance_container_iterator<iterator_type>;
        result.iterator_equal = &meta::detail::equal_container_iterator<iterator_type>;
        result.iterator_value = &meta::detail::set_iterator_value<iterator_type>;
      }

      result.value_descriptor = descriptor_ptr_of<element_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T>
[[nodiscard]] const meta::detail::associative_data* associative_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;
    static const meta::detail::associative_data data = [] {
      meta::detail::associative_data result{
          descriptor_type_id<key_type>(),
          descriptor_type_id<mapped_type>(),
          [](const void* storage) noexcept -> std::size_t {
            if (storage == nullptr) {
              return 0;
            }

            return traits::associative_traits<type>::size(*static_cast<const type*>(storage));
          },
          [](const void* storage, meta::detail::associative_raw_visitor visitor,
             void* user_data) -> bool {
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

      if constexpr (meta::detail::has_const_container_iterator_v<type>) {
        using iterator_type = decltype(std::declval<const type&>().begin());
        result.begin = &meta::detail::begin_container_iterator<type>;
        result.end = &meta::detail::end_container_iterator<type>;
        result.iterator_clone = &meta::detail::clone_container_iterator<iterator_type>;
        result.iterator_destroy = &meta::detail::destroy_container_iterator<iterator_type>;
        result.iterator_advance = &meta::detail::advance_container_iterator<iterator_type>;
        result.iterator_equal = &meta::detail::equal_container_iterator<iterator_type>;
        result.iterator_key = &meta::detail::associative_iterator_key<iterator_type>;
        result.iterator_value = &meta::detail::associative_iterator_value<iterator_type>;
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

template <typename T> [[nodiscard]] const meta::detail::pair_data* pair_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;
    static const meta::detail::pair_data data = [] {
      meta::detail::pair_data result{
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
[[nodiscard]] meta::detail::tuple_element_data make_tuple_element_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  return meta::detail::tuple_element_data{
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
[[nodiscard]] meta::detail::tuple_data make_tuple_descriptor_data(std::index_sequence<Indices...>) {
  meta::detail::tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_descriptor_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] const meta::detail::tuple_data* tuple_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    static const meta::detail::tuple_data data = make_tuple_descriptor_data<type>(
        std::make_index_sequence<traits::tuple_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Variant, std::size_t Index>
[[nodiscard]] meta::detail::variant_alternative_data make_variant_alternative_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  return meta::detail::variant_alternative_data{
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
[[nodiscard]] meta::detail::variant_data
make_variant_descriptor_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  meta::detail::variant_data data{};
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

template <typename T> [[nodiscard]] const meta::detail::variant_data* variant_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    static const meta::detail::variant_data data = make_variant_descriptor_data<type>(
        std::make_index_sequence<traits::variant_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> struct descriptor_holder {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const type_descriptor value{
      descriptor_type_id<type>(),
      sizeof(type),
      alignof(type),
      meta::detail::default_construct_fn_of<type>(),
      meta::detail::value_clone_fn_of<type>(),
      meta::detail::value_destroy_fn_of<type>(),
      meta::detail::value_assign_fn_of<type>(),
      semantics_ptr_of<type>(),
      indirect_data_ptr_of<type>(),
      sequence_data_ptr_of<type>(),
      set_data_ptr_of<type>(),
      associative_data_ptr_of<type>(),
      pair_data_ptr_of<type>(),
      tuple_data_ptr_of<type>(),
      variant_data_ptr_of<type>(),
  };
};

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &descriptor_holder<type>::value;
}

} // namespace caors::value::detail

#endif
