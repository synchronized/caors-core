// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_CONTAINER_REGISTRATION_H_
#define CAORS_META_CONTAINER_REGISTRATION_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {

template <typename T, typename = void> struct has_const_container_iterator : std::false_type {};

template <typename T>
struct has_const_container_iterator<T, std::void_t<decltype(std::declval<const T&>().begin()),
                                                   decltype(std::declval<const T&>().end())>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_const_container_iterator_v = has_const_container_iterator<T>::value;

template <typename Iterator>
[[nodiscard]] void* clone_container_iterator(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return new (std::nothrow) Iterator(*static_cast<const Iterator*>(iterator));
}

template <typename Iterator> void destroy_container_iterator(void* iterator) noexcept {
  delete static_cast<Iterator*>(iterator);
}

template <typename Iterator> void advance_container_iterator(void* iterator) noexcept {
  if (iterator != nullptr) {
    ++(*static_cast<Iterator*>(iterator));
  }
}

template <typename Iterator>
[[nodiscard]] bool equal_container_iterator(const void* lhs, const void* rhs) noexcept {
  if (lhs == nullptr || rhs == nullptr) {
    return lhs == rhs;
  }

  return *static_cast<const Iterator*>(lhs) == *static_cast<const Iterator*>(rhs);
}

template <typename Container>
[[nodiscard]] void* begin_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().begin());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->begin());
}

template <typename Container>
[[nodiscard]] void* end_container_iterator(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  using iterator_type = decltype(std::declval<const Container&>().end());
  return new (std::nothrow) iterator_type(static_cast<const Container*>(storage)->end());
}

template <typename Iterator>
[[nodiscard]] const void* set_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &**static_cast<const Iterator*>(iterator);
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_key(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->first;
}

template <typename Iterator>
[[nodiscard]] const void* associative_iterator_value(const void* iterator) noexcept {
  if (iterator == nullptr) {
    return nullptr;
  }

  return &(*static_cast<const Iterator*>(iterator))->second;
}

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

    meta_associative_data data{
        key,
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return traits::associative_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, meta_associative_raw_visitor visitor, void* user_data) -> bool {
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

    const error result = caors::meta::get_registry().emplace<meta_associative_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_pair_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;

    const type_id key = semantic_type_id<first_type>();
    const type_id value = semantic_type_id<second_type>();
    if (!id || !key || !value) {
      return {};
    }

    meta_pair_data data{
        key,
        value,
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

    const error result = caors::meta::get_registry().emplace<meta_pair_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename Tuple, std::size_t Index>
[[nodiscard]] meta_tuple_element_data make_tuple_element_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  return meta_tuple_element_data{
      semantic_type_id<element_type>(),
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
  };
}

template <typename Tuple, std::size_t... Indices>
[[nodiscard]] meta_tuple_data make_tuple_data(std::index_sequence<Indices...>) {
  meta_tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] error register_tuple_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    if (!id) {
      return {};
    }

    meta_tuple_data data =
        make_tuple_data<type>(std::make_index_sequence<traits::tuple_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = caors::meta::get_registry().emplace<meta_tuple_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename Variant, std::size_t Index>
[[nodiscard]] meta_variant_alternative_data make_variant_alternative_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  return meta_variant_alternative_data{
      semantic_type_id<alternative_type>(),
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
  };
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] meta_variant_data make_variant_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  meta_variant_data data{};
  data.alternatives.reserve(sizeof...(Indices));
  (data.alternatives.push_back(make_variant_alternative_data<type, Indices>()), ...);
  data.index = [](const void* storage) noexcept -> std::size_t {
    if (storage == nullptr) {
      return static_cast<std::size_t>(-1);
    }

    return traits::variant_traits<type>::index(*static_cast<const type*>(storage));
  };
  return data;
}

template <typename T> [[nodiscard]] error register_variant_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    if (!id) {
      return {};
    }

    meta_variant_data data =
        make_variant_data<type>(std::make_index_sequence<traits::variant_traits<type>::size>{});
    if (!data.valid()) {
      return {};
    }

    const error result = caors::meta::get_registry().emplace<meta_variant_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_sequence_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta_sequence_data data{
        value,
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

          return traits::sequence_traits<type>::element(*static_cast<const type*>(storage), index);
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

          return traits::sequence_traits<type>::append(*static_cast<type*>(storage),
                                                       *static_cast<const element_type*>(element));
        },
    };

    if constexpr (requires { traits::sequence_traits<type>::read_in_place; }) {
      data.read_in_place = traits::sequence_traits<type>::read_in_place;
    }

    if constexpr (requires { traits::sequence_traits<type>::can_read; }) {
      data.can_read = traits::sequence_traits<type>::can_read;
    }

    const error result = caors::meta::get_registry().emplace<meta_sequence_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_set_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;

    const type_id value = semantic_type_id<element_type>();
    if (!id || !value) {
      return {};
    }

    meta_set_data data{
        value,
        [](const void* storage) noexcept -> std::size_t {
          if (storage == nullptr) {
            return 0;
          }

          return traits::set_traits<type>::size(*static_cast<const type*>(storage));
        },
        [](const void* storage, meta_set_raw_visitor visitor, void* user_data) -> bool {
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

    const error result = caors::meta::get_registry().emplace<meta_set_data>(id, data);
    if (result && result.value() != errc::duplicate_component) {
      return result;
    }
  }

  return {};
}

template <typename T> [[nodiscard]] error register_container_data_for(type_id id) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    return register_sequence_data_for<type>(id);
  } else if constexpr (traits::set_traits<type>::is_set) {
    return register_set_data_for<type>(id);
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return register_associative_data_for<type>(id);
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return register_pair_data_for<type>(id);
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return register_tuple_data_for<type>(id);
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return register_variant_data_for<type>(id);
  }

  return {};
}

} // namespace caors::meta::detail

#endif
