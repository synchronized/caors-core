// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_OPS_MODEL_H_

#include "caors/meta/detail/container.h"
#include "caors/meta/detail/container_iterators.h"
#include "caors/meta/detail/type_capabilities.h"
#include "caors/meta/type_semantics.h"
#include "caors/traits/container.h"
#include "caors/traits/wrapper.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace caors::meta::detail {

struct type_descriptor;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;
template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept;

/** @brief 类型的内置互斥运行期能力。 */
using builtin_type_capability =
    std::variant<std::monostate, const enum_ops*, const indirect_ops*, const sequence_ops*,
                 const set_ops*, const associative_ops*, const pair_ops*, const tuple_ops*,
                 const variant_ops*>;

template <typename T> [[nodiscard]] const void* raw_pointer_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return *static_cast<T* const*>(storage);
}

template <typename T> [[nodiscard]] void* raw_pointer_mutable_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_const_v<T>) {
    return nullptr;
  } else {
    return *static_cast<T**>(storage);
  }
}

template <typename T> [[nodiscard]] bool raw_pointer_has_value(const void* storage) noexcept {
  return raw_pointer_value<T>(storage) != nullptr;
}

template <typename T> [[nodiscard]] type_id raw_pointer_dynamic_type(const void* storage) noexcept {
  using value_type = std::remove_cv_t<T>;
  const auto* value = static_cast<const value_type*>(raw_pointer_value<T>(storage));
  if (value == nullptr) {
    return type_id::invalid();
  }

  if constexpr (std::is_polymorphic_v<value_type>) {
    return type_id_of_cpp_type_info(typeid(*value));
  } else {
    return type_id_of_cpp_type<value_type>();
  }
}

template <typename T>
[[nodiscard]] const void* raw_pointer_dynamic_value(const void* storage) noexcept {
  const T* value = static_cast<const T*>(raw_pointer_value<T>(storage));
  if (value == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_polymorphic_v<std::remove_cv_t<T>>) {
    return dynamic_cast<const void*>(value);
  } else {
    return value;
  }
}

template <typename T>
[[nodiscard]] void* raw_pointer_mutable_dynamic_value(void* storage) noexcept {
  if constexpr (std::is_const_v<T>) {
    return nullptr;
  } else {
    T* value = static_cast<T*>(raw_pointer_mutable_value<T>(storage));
    if (value == nullptr) {
      return nullptr;
    }

    if constexpr (std::is_polymorphic_v<std::remove_cv_t<T>>) {
      return dynamic_cast<void*>(value);
    } else {
      return value;
    }
  }
}

template <typename T> bool raw_pointer_reset(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  *static_cast<T**>(storage) = nullptr;
  return true;
}

template <typename T> bool raw_pointer_ensure_value(void*, memory::memory_resource*) {
  return false;
}

template <typename Wrapper>
[[nodiscard]] const void* wrapper_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return traits::wrapper_access<Wrapper>::value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] void* wrapper_mutable_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return traits::wrapper_access<Wrapper>::mutable_value(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_has_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return traits::wrapper_access<Wrapper>::has_value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] type_id wrapper_dynamic_indirect_type(const void* storage) noexcept {
  using value_type = std::remove_cv_t<typename traits::wrapper_traits<Wrapper>::wrapped_type>;
  const auto* value = static_cast<const value_type*>(wrapper_indirect_value<Wrapper>(storage));
  if (value == nullptr) {
    return type_id::invalid();
  }

  if constexpr (std::is_polymorphic_v<value_type>) {
    return type_id_of_cpp_type_info(typeid(*value));
  } else {
    return type_id_of_cpp_type<value_type>();
  }
}

template <typename Wrapper>
[[nodiscard]] const void* wrapper_dynamic_indirect_value(const void* storage) noexcept {
  using value_type = std::remove_cv_t<typename traits::wrapper_traits<Wrapper>::wrapped_type>;
  const auto* value = static_cast<const value_type*>(wrapper_indirect_value<Wrapper>(storage));
  if (value == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_polymorphic_v<value_type>) {
    return dynamic_cast<const void*>(value);
  } else {
    return value;
  }
}

template <typename Wrapper>
[[nodiscard]] void* wrapper_mutable_dynamic_indirect_value(void* storage) noexcept {
  using value_type = std::remove_cv_t<typename traits::wrapper_traits<Wrapper>::wrapped_type>;
  auto* value = static_cast<value_type*>(wrapper_mutable_indirect_value<Wrapper>(storage));
  if (value == nullptr) {
    return nullptr;
  }

  if constexpr (std::is_polymorphic_v<value_type>) {
    return dynamic_cast<void*>(value);
  } else {
    return value;
  }
}

template <typename Wrapper> bool wrapper_reset_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return traits::wrapper_access<Wrapper>::reset(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper>
bool wrapper_ensure_indirect_value(void* storage, memory::memory_resource* resource) {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source, memory::memory_resource* value_resource) {
                  traits::wrapper_access<Wrapper>::ensure_value(source, value_resource);
                }) {
    return traits::wrapper_access<Wrapper>::ensure_value(*static_cast<Wrapper*>(storage), resource);
  } else if constexpr (requires(Wrapper& source) {
                         traits::wrapper_access<Wrapper>::ensure_value(source);
                       }) {
    return traits::wrapper_access<Wrapper>::ensure_value(*static_cast<Wrapper*>(storage));
  } else {
    return false;
  }
}

template <typename Wrapper> bool wrapper_set_indirect_value(void* storage, void* value) noexcept {
  if (storage == nullptr || value == nullptr) {
    return false;
  }

  using value_type = std::remove_cv_t<typename traits::wrapper_traits<Wrapper>::wrapped_type>;
  if constexpr (requires(Wrapper& source, value_type* stored) {
                  traits::wrapper_access<Wrapper>::set_value(source, stored);
                }) {
    return traits::wrapper_access<Wrapper>::set_value(*static_cast<Wrapper*>(storage),
                                                      static_cast<value_type*>(value));
  } else {
    return false;
  }
}

template <typename Wrapper>
bool wrapper_set_indirect_value_with_context(void* storage, void* value,
                                             const traits::wrapper_set_context& context) noexcept {
  if (storage == nullptr || value == nullptr) {
    return false;
  }

  using value_type = std::remove_cv_t<typename traits::wrapper_traits<Wrapper>::wrapped_type>;
  if constexpr (requires(Wrapper& source, value_type* stored,
                         const traits::wrapper_set_context& set_context) {
                  traits::wrapper_access<Wrapper>::set_value(source, stored, set_context);
                }) {
    return traits::wrapper_access<Wrapper>::set_value(*static_cast<Wrapper*>(storage),
                                                      static_cast<value_type*>(value), context);
  } else {
    return false;
  }
}

template <typename Enum> [[nodiscard]] std::int64_t enum_to_i64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::int64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum> [[nodiscard]] std::uint64_t enum_to_u64(const void* storage) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  return static_cast<std::uint64_t>(
      static_cast<underlying_type>(*static_cast<const Enum*>(storage)));
}

template <typename Enum>
[[nodiscard]] bool enum_from_i64(void* storage, std::int64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr) {
    return false;
  }

  if constexpr (std::is_signed_v<underlying_type>) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<underlying_type>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  } else {
    if (value < 0 || static_cast<std::uint64_t>(value) >
                         static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
      return false;
    }
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename Enum>
[[nodiscard]] bool enum_from_u64(void* storage, std::uint64_t value) noexcept {
  using underlying_type = std::underlying_type_t<Enum>;
  if (storage == nullptr ||
      value > static_cast<std::uint64_t>(std::numeric_limits<underlying_type>::max())) {
    return false;
  }

  *static_cast<Enum*>(storage) = static_cast<Enum>(static_cast<underlying_type>(value));
  return true;
}

template <typename T> [[nodiscard]] indirect_ops make_indirect_ops() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_pointer_t<type>;
    indirect_ops data{};
    data.value = &raw_pointer_value<value_type>;
    data.mutable_value = &raw_pointer_mutable_value<value_type>;
    data.dynamic_type = &raw_pointer_dynamic_type<value_type>;
    data.dynamic_value = &raw_pointer_dynamic_value<value_type>;
    data.mutable_dynamic_value = &raw_pointer_mutable_dynamic_value<value_type>;
    data.has_value = &raw_pointer_has_value<value_type>;
    data.reset = &raw_pointer_reset<value_type>;
    data.ensure_value = &raw_pointer_ensure_value<value_type>;
    return data;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper &&
                       traits::wrapper_traits<type>::semantics !=
                           traits::wrapper_semantics::unsupported) {
    indirect_ops data{};
    data.value = &wrapper_indirect_value<type>;
    data.mutable_value = &wrapper_mutable_indirect_value<type>;
    data.dynamic_type = &wrapper_dynamic_indirect_type<type>;
    data.dynamic_value = &wrapper_dynamic_indirect_value<type>;
    data.mutable_dynamic_value = &wrapper_mutable_dynamic_indirect_value<type>;
    data.has_value = &wrapper_has_indirect_value<type>;
    data.reset = &wrapper_reset_indirect_value<type>;
    data.ensure_value = &wrapper_ensure_indirect_value<type>;
    data.set_value = &wrapper_set_indirect_value<type>;
    data.set_value_with_context = &wrapper_set_indirect_value_with_context<type>;
    return data;
  } else {
    return {};
  }
}

template <typename T> [[nodiscard]] enum_ops make_enum_ops() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    using underlying_type = std::underlying_type_t<type>;
    return enum_ops{
        &enum_to_i64<type>,
        &enum_to_u64<type>,
        &enum_from_i64<type>,
        &enum_from_u64<type>,
        std::is_signed_v<underlying_type>,
    };
  } else {
    return {};
  }
}

template <typename T> struct enum_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const enum_ops value = make_enum_ops<type>();
};

template <typename T> [[nodiscard]] const enum_ops* enum_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    return enum_ops_model<type>::value.valid() ? &enum_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> struct indirect_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const indirect_ops value = [] {
    if constexpr (std::is_pointer_v<type>) {
      using value_type = std::remove_pointer_t<type>;

      indirect_ops result = make_indirect_ops<type>();
      result.value_descriptor = descriptor_ptr_of<std::remove_cv_t<value_type>>();
      return result;
    } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
      using value_type = std::remove_cv_t<typename traits::wrapper_traits<type>::wrapped_type>;

      indirect_ops result{};
      if constexpr (traits::wrapper_traits<type>::semantics !=
                    traits::wrapper_semantics::unsupported) {
        result = make_indirect_ops<type>();
      }
      result.value_descriptor = descriptor_ptr_of<value_type>();
      return result;
    } else {
      return indirect_ops{};
    }
  }();
};

template <typename T> [[nodiscard]] const indirect_ops* indirect_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    return indirect_ops_model<type>::value.valid() ? &indirect_ops_model<type>::value : nullptr;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    return indirect_ops_model<type>::value.value_descriptor != nullptr
               ? &indirect_ops_model<type>::value
               : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> struct sequence_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::sequence_traits<type>::element_type;

  inline static const sequence_ops value = [] {
    sequence_ops result{
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
        [](void* storage, std::size_t size, memory::memory_resource* resource) -> bool {
          if (storage == nullptr) {
            return false;
          }

          if constexpr (requires(type& values, std::size_t count,
                                 memory::memory_resource* resize_resource) {
                          traits::sequence_traits<type>::resize(values, count, resize_resource);
                        }) {
            return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size,
                                                         resource);
          } else {
            (void)resource;
            return traits::sequence_traits<type>::resize(*static_cast<type*>(storage), size);
          }
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
      result.read_in_place = traits::sequence_traits<type>::read_in_place;
    }

    if constexpr (requires { traits::sequence_traits<type>::can_read; }) {
      result.can_read = traits::sequence_traits<type>::can_read;
    }

    if constexpr (requires { traits::sequence_traits<type>::fixed_size; }) {
      result.fixed_size = traits::sequence_traits<type>::fixed_size;
    }

    result.value_descriptor = descriptor_ptr_of<element_type>();
    return result;
  }();
};

template <typename T> [[nodiscard]] const sequence_ops* sequence_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_ops_model<type>::value.valid() ? &sequence_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

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

template <typename T> struct pair_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

  inline static const pair_ops value = [] {
    pair_ops result{
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
};

template <typename T> [[nodiscard]] const pair_ops* pair_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_ops_model<type>::value.valid() ? &pair_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Tuple, std::size_t Index> struct tuple_element_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  inline static const tuple_element_ops value{
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
};

template <typename T> struct tuple_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  template <std::size_t... Indices>
  [[nodiscard]] static tuple_ops make(std::index_sequence<Indices...>) {
    tuple_ops data{};
    data.elements.reserve(sizeof...(Indices));
    (data.elements.push_back(tuple_element_ops_model<type, Indices>::value), ...);
    return data;
  }

  inline static const tuple_ops value =
      make(std::make_index_sequence<traits::tuple_traits<type>::size>{});
};

template <typename T> [[nodiscard]] const tuple_ops* tuple_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_ops_model<type>::value.valid() ? &tuple_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Variant, std::size_t Index> struct variant_alternative_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  inline static const variant_alternative_ops value{
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
      [](void* storage, memory::memory_resource* resource) -> bool {
        if (storage == nullptr) {
          return false;
        }

        if constexpr (requires(type& values, memory::memory_resource* emplace_resource) {
                        traits::variant_traits<type>::template emplace<Index>(values,
                                                                              emplace_resource);
                      }) {
          return traits::variant_traits<type>::template emplace<Index>(*static_cast<type*>(storage),
                                                                       resource);
        } else {
          (void)resource;
          return traits::variant_traits<type>::template emplace<Index>(
              *static_cast<type*>(storage));
        }
      },
      descriptor_ptr_of<alternative_type>(),
  };
};

template <typename T> struct variant_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  template <std::size_t... Indices>
  [[nodiscard]] static variant_ops make(std::index_sequence<Indices...>) {
    variant_ops data{};
    data.alternatives.reserve(sizeof...(Indices));
    (data.alternatives.push_back(variant_alternative_ops_model<type, Indices>::value), ...);
    data.index = [](const void* storage) noexcept -> std::size_t {
      if (storage == nullptr) {
        return static_cast<std::size_t>(-1);
      }

      return traits::variant_traits<type>::index(*static_cast<const type*>(storage));
    };
    return data;
  }

  inline static const variant_ops value =
      make(std::make_index_sequence<traits::variant_traits<type>::size>{});
};

template <typename T> [[nodiscard]] const variant_ops* variant_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_ops_model<type>::value.valid() ? &variant_ops_model<type>::value : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] builtin_type_capability builtin_type_capability_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    return indirect_ops_ptr_of<type>();
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_ops_ptr_of<type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    return set_ops_ptr_of<type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return associative_ops_ptr_of<type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_ops_ptr_of<type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_ops_ptr_of<type>();
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_ops_ptr_of<type>();
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    return indirect_ops_ptr_of<type>();
  } else if constexpr (std::is_enum_v<type>) {
    return enum_ops_ptr_of<type>();
  } else {
    return std::monostate{};
  }
}

} // namespace caors::meta::detail

#endif
