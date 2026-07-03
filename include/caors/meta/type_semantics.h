// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_SEMANTICS_H_
#define CAORS_META_TYPE_SEMANTICS_H_

#include "caors/meta/registry.h"

namespace caors {

namespace detail {

template <typename T>
inline constexpr bool is_supported_wrapper_v = wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

template <typename T>
inline constexpr bool is_nested_type_indirection_v =
    std::is_pointer_v<std::remove_cv_t<T>> || is_supported_wrapper_v<T>;

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

template <typename T> bool raw_pointer_reset(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  *static_cast<T**>(storage) = nullptr;
  return true;
}

template <typename T> bool raw_pointer_ensure_value(void*) { return false; }

template <typename Wrapper>
[[nodiscard]] const void* wrapper_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] void* wrapper_mutable_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  return wrapper_access<Wrapper>::mutable_value(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_has_indirect_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::has_value(*static_cast<const Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_reset_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return wrapper_access<Wrapper>::reset(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_ensure_indirect_value(void* storage) {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source) { wrapper_access<Wrapper>::ensure_value(source); }) {
    return wrapper_access<Wrapper>::ensure_value(*static_cast<Wrapper*>(storage));
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

template <typename Wrapper>
[[nodiscard]] object* wrapper_object_ref_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (requires(const Wrapper& source) {
                  wrapper_access<Wrapper>::object_value(source);
                }) {
    using result_type =
        decltype(wrapper_access<Wrapper>::object_value(std::declval<const Wrapper&>()));
    if constexpr (std::is_convertible_v<result_type, object*>) {
      return wrapper_access<Wrapper>::object_value(*static_cast<const Wrapper*>(storage));
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_set_object_ref(void* storage, object* instance) noexcept {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source, object* object_instance) {
                  wrapper_access<Wrapper>::set_object(source, object_instance);
                }) {
    using result_type = decltype(wrapper_access<Wrapper>::set_object(std::declval<Wrapper&>(),
                                                                     std::declval<object*>()));
    if constexpr (std::is_convertible_v<result_type, bool>) {
      return wrapper_access<Wrapper>::set_object(*static_cast<Wrapper*>(storage), instance);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] type_id semantic_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (has_type_name_v<type>) {
    const error result = meta::get_registry().register_type_for<type>(
        meta::make_type_data<type>(type_name<type>::value));
    if (result && result.value() != errc::duplicate_id) {
      return type_id::invalid();
    }
  }

  const std::optional<meta::type_view> registered = meta::get_registry().find_type<type>();
  return registered.has_value() ? registered->id() : type_id::invalid();
}

template <typename Type> struct type_semantics_traits {
  using type = std::remove_cv_t<std::remove_reference_t<Type>>;

  [[nodiscard]] static meta::type_semantics_data get() {
    meta::type_semantics_data info{};
    if constexpr (adapters::sequence_traits<type>::is_sequence) {
      using element_type = typename adapters::sequence_traits<type>::element_type;

      info.kind = meta::type_kind::sequence;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (adapters::set_traits<type>::is_set) {
      using element_type = typename adapters::set_traits<type>::element_type;

      info.kind = meta::type_kind::set;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (adapters::associative_traits<type>::is_associative) {
      using key_type = typename adapters::associative_traits<type>::key_type;
      using mapped_type = typename adapters::associative_traits<type>::mapped_type;

      info.kind = meta::type_kind::associative;
      info.key_type = semantic_type_id<key_type>();
      info.value_type = semantic_type_id<mapped_type>();
      return info;
    } else if constexpr (adapters::pair_traits<type>::is_pair) {
      using first_type = typename adapters::pair_traits<type>::first_type;
      using second_type = typename adapters::pair_traits<type>::second_type;

      info.kind = meta::type_kind::pair;
      info.key_type = semantic_type_id<first_type>();
      info.value_type = semantic_type_id<second_type>();
      return info;
    } else if constexpr (adapters::tuple_traits<type>::is_tuple) {
      info.kind = meta::type_kind::tuple;
      return info;
    } else if constexpr (adapters::variant_traits<type>::is_variant) {
      info.kind = meta::type_kind::variant;
      return info;
    } else if constexpr (wrapper_traits<type>::is_wrapper) {
      using wrapped_type = typename wrapper_traits<type>::wrapped_type;
      using value_type = std::remove_cv_t<wrapped_type>;

      static_assert(!is_nested_type_indirection_v<value_type>,
                    "CAORS meta supports at most one pointer or wrapper level");

      info.kind = meta::type_kind::wrapper;
      info.wrapper_semantics = wrapper_traits<type>::semantics;
      info.value_type = semantic_type_id<value_type>();
      info.indirect_value = &wrapper_indirect_value<type>;
      info.mutable_indirect_value = &wrapper_mutable_indirect_value<type>;
      info.has_indirect_value = &wrapper_has_indirect_value<type>;
      info.reset_indirect_value = &wrapper_reset_indirect_value<type>;
      info.ensure_indirect_value = &wrapper_ensure_indirect_value<type>;
      if constexpr (wrapper_traits<type>::semantics == meta::wrapper_semantics::object_ref) {
        info.get_object_ref = &wrapper_object_ref_value<type>;
        info.set_object_ref = &wrapper_set_object_ref<type>;
      }
      info.nullable = wrapper_traits<type>::nullable;
      return info;
    } else {
      if constexpr (std::is_enum_v<type>) {
        using underlying_type = std::underlying_type_t<type>;
        info.kind = meta::type_kind::enumeration;
        info.enum_to_i64 = &enum_to_i64<type>;
        info.enum_to_u64 = &enum_to_u64<type>;
        info.enum_from_i64 = &enum_from_i64<type>;
        info.enum_from_u64 = &enum_from_u64<type>;
        info.enum_signed = std::is_signed_v<underlying_type>;
      } else {
        info.kind = meta::type_kind::value;
      }
      return info;
    }
  }
};

template <typename T> struct type_semantics_traits<T*> {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_type_indirection_v<value_type>,
                "CAORS meta supports at most one pointer or wrapper level");

  [[nodiscard]] static meta::type_semantics_data get() {
    meta::type_semantics_data info{};
    info.kind = meta::type_kind::raw_pointer;
    info.wrapper_semantics = meta::wrapper_semantics::inline_value;
    info.value_type = semantic_type_id<value_type>();
    info.indirect_value = &raw_pointer_value<T>;
    info.mutable_indirect_value = &raw_pointer_mutable_value<T>;
    info.has_indirect_value = &raw_pointer_has_value<T>;
    info.reset_indirect_value = &raw_pointer_reset<T>;
    info.ensure_indirect_value = &raw_pointer_ensure_value<T>;
    info.pointer_depth = 1;
    info.nullable = true;
    return info;
  }
};

template <typename T> [[nodiscard]] meta::type_semantics_data make_type_semantics_data() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return type_semantics_traits<type>::get();
}

template <typename T> [[nodiscard]] error ensure_type_semantics_for(type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (meta::get_registry().has<meta::type_semantics_data>(id)) {
    return {};
  }

  meta::type_semantics_data semantics = make_type_semantics_data<T>();
  if (!semantics.value_type && semantics.kind != meta::type_kind::tuple &&
      semantics.kind != meta::type_kind::variant) {
    semantics.value_type = id;
  }

  const error result = meta::get_registry().emplace<meta::type_semantics_data>(id, semantics);
  if (result && result.value() != errc::duplicate_component) {
    return result;
  }

  return {};
}

} // namespace detail

} // namespace caors

#endif
