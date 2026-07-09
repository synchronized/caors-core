// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_INDIRECT_OPS_H_
#define CAORS_META_DETAIL_BUILTIN_INDIRECT_OPS_H_

#include "caors/meta/detail/builtin/capability.h"
#include "caors/traits/wrapper.h"

#include <type_traits>
#include <typeinfo>

namespace caors::meta::detail {

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

} // namespace caors::meta::detail

#endif
