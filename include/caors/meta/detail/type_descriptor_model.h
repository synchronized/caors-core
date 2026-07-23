// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_MODEL_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_MODEL_H_

#include "caors/meta/detail/builtin_ops_model.h"
#include "caors/meta/detail/type_descriptor.h"
#include "caors/meta/detail/type_descriptor_name.h"
#include "caors/meta/detail/value_ops_model.h"
#include "caors/meta/type_semantics.h"
#include "caors/traits/container.h"
#include "caors/traits/wrapper.h"

#include <type_traits>

namespace caors::meta::detail {

template <typename T> static void fill_descriptor_semantics(type_descriptor& descriptor) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_cv_t<std::remove_pointer_t<type>>;

    static_assert(!meta::is_nested_type_indirection_v<value_type>,
                  "CAORS value supports at most one pointer or wrapper level");

    descriptor.semantics.kind = type_kind::raw_pointer;
    descriptor.semantics.wrapper = meta::wrapper_semantics::inline_value;
    descriptor.semantics.indirect = true;
    descriptor.semantics.nullable = true;
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    descriptor.semantics.kind = type_kind::sequence;
  } else if constexpr (traits::set_traits<type>::is_set) {
    descriptor.semantics.kind = type_kind::set;
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    descriptor.semantics.kind = type_kind::associative;
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    descriptor.semantics.kind = type_kind::pair;
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    descriptor.semantics.kind = type_kind::tuple;
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    descriptor.semantics.kind = type_kind::variant;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    using wrapped_type = typename traits::wrapper_traits<type>::wrapped_type;
    using value_type = std::remove_cv_t<wrapped_type>;

    static_assert(!meta::is_nested_type_indirection_v<value_type>,
                  "CAORS value supports at most one pointer or wrapper level");

    descriptor.semantics.kind = type_kind::wrapper;
    descriptor.semantics.wrapper = traits::wrapper_traits<type>::semantics;
    descriptor.semantics.indirect = true;
    descriptor.semantics.nullable = traits::wrapper_traits<type>::nullable;
  } else {
    if constexpr (std::is_enum_v<type>) {
      descriptor.semantics.kind = type_kind::enumeration;
    } else {
      descriptor.semantics.kind = type_kind::value;
    }
  }
}

template <typename T> [[nodiscard]] static type_descriptor make_type_descriptor() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  type_descriptor result{};
  result.type = descriptor_type_id<type>();
  result.name = descriptor_type_name<type>();
  result.value = meta::value_storage_ops{
      meta::type_layout{sizeof(type), alignof(type)},
      meta::value_ops{
          default_construct_fn_of<type>(),
          value_clone_fn_of<type>(),
          value_destroy_fn_of<type>(),
          value_assign_fn_of<type>(),
          resource_default_construct_fn_of<type>(),
          resource_destroy_fn_of<type>(),
      },
      meta::value_traits{
          std::is_trivially_copyable_v<type>,
          std::is_trivially_destructible_v<type>,
          std::is_trivially_default_constructible_v<type>,
      },
  };
  fill_descriptor_semantics<type>(result);
  result.builtin = builtin_ops_of<type>();
  result.static_capabilities = meta::capabilities::static_capabilities_data<type>();
  result.static_capability_count = meta::capabilities::static_capabilities_size<type>();
  return result;
}

template <typename T> [[nodiscard]] static const type_descriptor* descriptor_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const type_descriptor descriptor = make_type_descriptor<type>();
  return &descriptor;
}

template <typename T> [[nodiscard]] error ensure_type_descriptor_for() {
  const type_descriptor* descriptor = descriptor_ptr_of<T>();
  if (descriptor == nullptr || !descriptor->valid()) {
    return {error_domain::meta, errc::invalid_type};
  }

  return ensure_type_descriptor_registered(*descriptor);
}

} // namespace caors::meta::detail

#endif
