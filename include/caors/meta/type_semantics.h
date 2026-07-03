// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_SEMANTICS_H_
#define CAORS_META_TYPE_SEMANTICS_H_

#include "caors/meta/registry.h"

namespace caors::meta::detail {

template <typename T>
inline constexpr bool is_supported_wrapper_v =
    traits::wrapper_traits<std::remove_cv_t<T>>::is_wrapper;

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

template <typename Wrapper> bool wrapper_reset_indirect_value(void* storage) noexcept {
  if (storage == nullptr) {
    return false;
  }

  return traits::wrapper_access<Wrapper>::reset(*static_cast<Wrapper*>(storage));
}

template <typename Wrapper> bool wrapper_ensure_indirect_value(void* storage) {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source) {
                  traits::wrapper_access<Wrapper>::ensure_value(source);
                }) {
    return traits::wrapper_access<Wrapper>::ensure_value(*static_cast<Wrapper*>(storage));
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
[[nodiscard]] objects::object* wrapper_object_ref_value(const void* storage) noexcept {
  if (storage == nullptr) {
    return nullptr;
  }

  if constexpr (requires(const Wrapper& source) {
                  traits::wrapper_access<Wrapper>::object_value(source);
                }) {
    using result_type =
        decltype(traits::wrapper_access<Wrapper>::object_value(std::declval<const Wrapper&>()));
    if constexpr (std::is_convertible_v<result_type, objects::object*>) {
      return traits::wrapper_access<Wrapper>::object_value(*static_cast<const Wrapper*>(storage));
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

template <typename Wrapper>
[[nodiscard]] bool wrapper_set_object_ref(void* storage, objects::object* instance) noexcept {
  if (storage == nullptr) {
    return false;
  }

  if constexpr (requires(Wrapper& source, objects::object* object_instance) {
                  traits::wrapper_access<Wrapper>::set_object(source, object_instance);
                }) {
    using result_type = decltype(traits::wrapper_access<Wrapper>::set_object(
        std::declval<Wrapper&>(), std::declval<objects::object*>()));
    if constexpr (std::is_convertible_v<result_type, bool>) {
      return traits::wrapper_access<Wrapper>::set_object(*static_cast<Wrapper*>(storage), instance);
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename T> [[nodiscard]] type_id semantic_type_id() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const ::caors::meta::type registered = caors::meta::get_registry().find_type<type>();
  return registered ? registered.id() : type_id::invalid();
}

template <typename Type> struct type_semantics_traits {
  using type = std::remove_cv_t<std::remove_reference_t<Type>>;

  [[nodiscard]] static type_semantics_data get() {
    type_semantics_data info{};
    if constexpr (traits::sequence_traits<type>::is_sequence) {
      using element_type = typename traits::sequence_traits<type>::element_type;

      info.kind = type_kind::sequence;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (traits::set_traits<type>::is_set) {
      using element_type = typename traits::set_traits<type>::element_type;

      info.kind = type_kind::set;
      info.value_type = semantic_type_id<element_type>();
      return info;
    } else if constexpr (traits::associative_traits<type>::is_associative) {
      using key_type = typename traits::associative_traits<type>::key_type;
      using mapped_type = typename traits::associative_traits<type>::mapped_type;

      info.kind = type_kind::associative;
      info.key_type = semantic_type_id<key_type>();
      info.value_type = semantic_type_id<mapped_type>();
      return info;
    } else if constexpr (traits::pair_traits<type>::is_pair) {
      using first_type = typename traits::pair_traits<type>::first_type;
      using second_type = typename traits::pair_traits<type>::second_type;

      info.kind = type_kind::pair;
      info.key_type = semantic_type_id<first_type>();
      info.value_type = semantic_type_id<second_type>();
      return info;
    } else if constexpr (traits::tuple_traits<type>::is_tuple) {
      info.kind = type_kind::tuple;
      return info;
    } else if constexpr (traits::variant_traits<type>::is_variant) {
      info.kind = type_kind::variant;
      return info;
    } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
      using wrapped_type = typename traits::wrapper_traits<type>::wrapped_type;
      using value_type = std::remove_cv_t<wrapped_type>;

      static_assert(!is_nested_type_indirection_v<value_type>,
                    "CAORS meta supports at most one pointer or wrapper level");

      info.kind = type_kind::wrapper;
      info.wrapper_semantics = traits::wrapper_traits<type>::semantics;
      info.value_type = semantic_type_id<value_type>();
      info.indirect_value = &wrapper_indirect_value<type>;
      info.mutable_indirect_value = &wrapper_mutable_indirect_value<type>;
      info.has_indirect_value = &wrapper_has_indirect_value<type>;
      info.reset_indirect_value = &wrapper_reset_indirect_value<type>;
      info.ensure_indirect_value = &wrapper_ensure_indirect_value<type>;
      if constexpr (traits::wrapper_traits<type>::semantics == wrapper_semantics::object_ref) {
        info.get_object_ref = &wrapper_object_ref_value<type>;
        info.set_object_ref = &wrapper_set_object_ref<type>;
      }
      info.nullable = traits::wrapper_traits<type>::nullable;
      return info;
    } else {
      if constexpr (std::is_enum_v<type>) {
        using underlying_type = std::underlying_type_t<type>;
        info.kind = type_kind::enumeration;
        info.enum_to_i64 = &enum_to_i64<type>;
        info.enum_to_u64 = &enum_to_u64<type>;
        info.enum_from_i64 = &enum_from_i64<type>;
        info.enum_from_u64 = &enum_from_u64<type>;
        info.enum_signed = std::is_signed_v<underlying_type>;
      } else {
        info.kind = type_kind::value;
      }
      return info;
    }
  }
};

template <typename T> struct type_semantics_traits<T*> {
  using value_type = std::remove_cv_t<T>;

  static_assert(!is_nested_type_indirection_v<value_type>,
                "CAORS meta supports at most one pointer or wrapper level");

  [[nodiscard]] static type_semantics_data get() {
    type_semantics_data info{};
    info.kind = type_kind::raw_pointer;
    info.wrapper_semantics = wrapper_semantics::inline_value;
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

template <typename T> [[nodiscard]] type_semantics_data make_type_semantics_data() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return type_semantics_traits<type>::get();
}

template <typename T> [[nodiscard]] error ensure_type_semantics_for(type_id id) {
  if (!id) {
    return {error_domain::meta, errc::invalid_type};
  }

  if (caors::meta::get_registry().has<type_semantics_data>(id)) {
    return {};
  }

  type_semantics_data semantics = make_type_semantics_data<T>();
  if (!semantics.value_type && semantics.kind != type_kind::tuple &&
      semantics.kind != type_kind::variant) {
    semantics.value_type = id;
  }

  const error result = caors::meta::get_registry().emplace<type_semantics_data>(id, semantics);
  if (result && result.value() != errc::duplicate_component) {
    return result;
  }

  return {};
}

} // namespace caors::meta::detail

#endif
