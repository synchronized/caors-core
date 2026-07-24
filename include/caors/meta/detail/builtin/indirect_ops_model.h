// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_BUILTIN_INDIRECT_OPS_MODEL_H_
#define CAORS_META_DETAIL_BUILTIN_INDIRECT_OPS_MODEL_H_

#include "caors/memory/resource.h"
#include "caors/meta/detail/builtin/ops.h"
#include "caors/meta/detail/type_view.h"
#include "caors/traits/wrapper.h"

#include <type_traits>
#include <typeinfo>

namespace caors::meta::detail {

using meta::indirect_ops;

template <typename T> struct raw_pointer_ops_model {
  using value_type = T;
  using non_cv_value_type = std::remove_cv_t<value_type>;

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return make_type_view(static_type_id<non_cv_value_type>(),
                          static_type_record_ptr_of<non_cv_value_type>());
  }

  [[nodiscard]] static const void* value(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return *static_cast<value_type* const*>(storage);
  }

  [[nodiscard]] static void* mutable_value(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    if constexpr (std::is_const_v<value_type>) {
      return nullptr;
    } else {
      return *static_cast<value_type**>(storage);
    }
  }

  [[nodiscard]] static bool has_value(const void* storage) noexcept {
    return value(storage) != nullptr;
  }

  [[nodiscard]] static type_id dynamic_type(const void* storage) noexcept {
    const auto* object = static_cast<const non_cv_value_type*>(value(storage));
    if (object == nullptr) {
      return type_id::invalid();
    }

    if constexpr (std::is_polymorphic_v<non_cv_value_type>) {
      return type_id_of_cpp_type_info(typeid(*object));
    } else {
      return type_id_of_cpp_type<non_cv_value_type>();
    }
  }

  [[nodiscard]] static const void* dynamic_value(const void* storage) noexcept {
    const value_type* object = static_cast<const value_type*>(value(storage));
    if (object == nullptr) {
      return nullptr;
    }

    if constexpr (std::is_polymorphic_v<non_cv_value_type>) {
      return dynamic_cast<const void*>(object);
    } else {
      return object;
    }
  }

  [[nodiscard]] static void* mutable_dynamic_value(void* storage) noexcept {
    if constexpr (std::is_const_v<value_type>) {
      return nullptr;
    } else {
      value_type* object = static_cast<value_type*>(mutable_value(storage));
      if (object == nullptr) {
        return nullptr;
      }

      if constexpr (std::is_polymorphic_v<non_cv_value_type>) {
        return dynamic_cast<void*>(object);
      } else {
        return object;
      }
    }
  }

  static bool reset(void* storage) noexcept {
    if (storage == nullptr) {
      return false;
    }

    *static_cast<value_type**>(storage) = nullptr;
    return true;
  }

  static bool ensure_value(void*, memory::memory_resource*) { return false; }

  inline static const indirect_ops value_ops{
      &value,     &mutable_value,  &dynamic_type, &dynamic_value, &mutable_dynamic_value,
      &has_value, &reset,          &ensure_value, nullptr,        nullptr,
      nullptr,    &get_value_type,
  };
};

template <typename Wrapper> struct wrapper_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<Wrapper>>;
  using value_type = std::remove_cv_t<typename traits::wrapper_traits<type>::wrapped_type>;

  [[nodiscard]] static meta::type get_value_type() noexcept {
    return make_type_view(static_type_id<value_type>(), static_type_record_ptr_of<value_type>());
  }

  [[nodiscard]] static const void* value(const void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::wrapper_access<type>::value(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static void* mutable_value(void* storage) noexcept {
    if (storage == nullptr) {
      return nullptr;
    }

    return traits::wrapper_access<type>::mutable_value(*static_cast<type*>(storage));
  }

  [[nodiscard]] static bool has_value(const void* storage) noexcept {
    if (storage == nullptr) {
      return false;
    }

    return traits::wrapper_access<type>::has_value(*static_cast<const type*>(storage));
  }

  [[nodiscard]] static type_id dynamic_type(const void* storage) noexcept {
    const auto* object = static_cast<const value_type*>(value(storage));
    if (object == nullptr) {
      return type_id::invalid();
    }

    if constexpr (std::is_polymorphic_v<value_type>) {
      return type_id_of_cpp_type_info(typeid(*object));
    } else {
      return type_id_of_cpp_type<value_type>();
    }
  }

  [[nodiscard]] static const void* dynamic_value(const void* storage) noexcept {
    const auto* object = static_cast<const value_type*>(value(storage));
    if (object == nullptr) {
      return nullptr;
    }

    if constexpr (std::is_polymorphic_v<value_type>) {
      return dynamic_cast<const void*>(object);
    } else {
      return object;
    }
  }

  [[nodiscard]] static void* mutable_dynamic_value(void* storage) noexcept {
    auto* object = static_cast<value_type*>(mutable_value(storage));
    if (object == nullptr) {
      return nullptr;
    }

    if constexpr (std::is_polymorphic_v<value_type>) {
      return dynamic_cast<void*>(object);
    } else {
      return object;
    }
  }

  static bool reset(void* storage) noexcept {
    if (storage == nullptr) {
      return false;
    }

    return traits::wrapper_access<type>::reset(*static_cast<type*>(storage));
  }

  static bool ensure_value(void* storage, memory::memory_resource* resource) {
    if (storage == nullptr) {
      return false;
    }

    if constexpr (requires(type& source, memory::memory_resource* value_resource) {
                    traits::wrapper_access<type>::ensure_value(source, value_resource);
                  }) {
      return traits::wrapper_access<type>::ensure_value(*static_cast<type*>(storage), resource);
    } else if constexpr (requires(type& source) {
                           traits::wrapper_access<type>::ensure_value(source);
                         }) {
      return traits::wrapper_access<type>::ensure_value(*static_cast<type*>(storage));
    } else {
      return false;
    }
  }

  static bool set_value(void* storage, void* object) noexcept {
    if (storage == nullptr || object == nullptr) {
      return false;
    }

    if constexpr (requires(type& source, value_type* stored) {
                    traits::wrapper_access<type>::set_value(source, stored);
                  }) {
      return traits::wrapper_access<type>::set_value(*static_cast<type*>(storage),
                                                     static_cast<value_type*>(object));
    } else {
      return false;
    }
  }

  static bool emplace_value(void* storage, void* object) {
    if (storage == nullptr || object == nullptr) {
      return false;
    }

    if constexpr (requires(type& source, value_type* stored) {
                    traits::wrapper_access<type>::emplace_value(source, stored);
                  }) {
      return traits::wrapper_access<type>::emplace_value(*static_cast<type*>(storage),
                                                         static_cast<value_type*>(object));
    } else {
      return false;
    }
  }

  static bool set_value_with_context(void* storage, void* object,
                                     const traits::wrapper_set_context& context) noexcept {
    if (storage == nullptr || object == nullptr) {
      return false;
    }

    if constexpr (requires(type& source, value_type* stored,
                           const traits::wrapper_set_context& set_context) {
                    traits::wrapper_access<type>::set_value(source, stored, set_context);
                  }) {
      return traits::wrapper_access<type>::set_value(*static_cast<type*>(storage),
                                                     static_cast<value_type*>(object), context);
    } else {
      return false;
    }
  }

  [[nodiscard]] static indirect_ops make() {
    indirect_ops result{};
    if constexpr (traits::wrapper_traits<type>::semantics !=
                  traits::wrapper_semantics::unsupported) {
      result.value = &value;
      result.mutable_value = &mutable_value;
      result.dynamic_type = &dynamic_type;
      result.dynamic_value = &dynamic_value;
      result.mutable_dynamic_value = &mutable_dynamic_value;
      result.has_value = &has_value;
      result.reset = &reset;
      result.ensure_value = &ensure_value;
      if constexpr (requires(type& source, value_type* stored) {
                      traits::wrapper_access<type>::emplace_value(source, stored);
                    }) {
        result.emplace_value = &emplace_value;
      }
      result.set_value = &set_value;
      result.set_value_with_context = &set_value_with_context;
    }

    result.get_value_type = &get_value_type;
    return result;
  }

  inline static const indirect_ops value_ops = make();
};

template <typename T> struct indirect_ops_model {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  [[nodiscard]] static indirect_ops make() {
    if constexpr (std::is_pointer_v<type>) {
      using value_type = std::remove_pointer_t<type>;
      return raw_pointer_ops_model<value_type>::value_ops;
    } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
      return wrapper_ops_model<type>::value_ops;
    } else {
      return {};
    }
  }

  inline static const indirect_ops value = make();
};

template <typename T> [[nodiscard]] const indirect_ops* indirect_ops_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    return indirect_ops_model<type>::value.valid() ? &indirect_ops_model<type>::value : nullptr;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    static const indirect_ops ops = wrapper_ops_model<type>::make();
    return ops.get_value_type != nullptr ? &ops : nullptr;
  } else {
    return nullptr;
  }
}

} // namespace caors::meta::detail

#endif
