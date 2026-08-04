// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_STATIC_TYPE_RECORD_MODEL_H_
#define CAORS_META_DETAIL_STATIC_TYPE_RECORD_MODEL_H_

#include "caors/adapters/small_vector_traits.h"
#include "caors/adapters/stl_traits.h"
#include "caors/meta/detail/builtin_ops_model.h"
#include "caors/meta/detail/static_type_name.h"
#include "caors/meta/detail/type_record.h"
#include "caors/meta/detail/value_ops_model.h"
#include "caors/meta/type_semantics.h"
#include "caors/traits/container.h"
#include "caors/traits/wrapper.h"

#include <type_traits>

namespace caors::meta::detail {

template <typename T> [[nodiscard]] const type_record* static_type_record_ptr_of() noexcept;

template <typename T> [[nodiscard]] type_relation static_type_relation() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return type_relation{static_type_id<type>(), static_type_record_ptr_of<type>()};
}

template <typename T> [[nodiscard]] type_semantics static_type_semantics() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  type_semantics result{};

  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_cv_t<std::remove_pointer_t<type>>;

    result.kind = type_kind::raw_pointer;
    result.wrapper = meta::wrapper_semantics::inline_value;
    result.value_type = static_type_relation<value_type>();
    result.indirect = true;
    result.nullable = true;
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    result.kind = type_kind::sequence;
    result.value_type =
        static_type_relation<typename traits::sequence_traits<type>::element_type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    result.kind = type_kind::set;
    result.value_type = static_type_relation<typename traits::set_traits<type>::element_type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    result.kind = type_kind::associative;
    result.key_type = static_type_relation<typename traits::associative_traits<type>::key_type>();
    result.value_type =
        static_type_relation<typename traits::associative_traits<type>::mapped_type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    result.kind = type_kind::pair;
    result.key_type = static_type_relation<typename traits::pair_traits<type>::first_type>();
    result.value_type = static_type_relation<typename traits::pair_traits<type>::second_type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    result.kind = type_kind::tuple;
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    result.kind = type_kind::variant;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    using wrapped_type = typename traits::wrapper_traits<type>::wrapped_type;
    using value_type = std::remove_cv_t<wrapped_type>;

    result.kind = type_kind::wrapper;
    result.wrapper = traits::wrapper_traits<type>::semantics;
    result.value_type = static_type_relation<value_type>();
    result.indirect = true;
    result.nullable = traits::wrapper_traits<type>::nullable;
  } else {
    if constexpr (std::is_enum_v<type>) {
      result.kind = type_kind::enumeration;
    } else {
      result.kind = type_kind::value;
    }
  }

  return result;
}

template <typename T> [[nodiscard]] type_record make_static_type_record() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  const meta::value_storage_ops value{
      meta::type_layout{sizeof(type), alignof(type)},
      meta::value_ops{
          default_construct_fn_of<type>(),
          value_clone_fn_of<type>(),
          value_resource_clone_fn_of<type>(),
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
  return type_record{
      static_type_id<type>(),
      static_type_name<type>(),
      traits::type_identity<type>::guid(),
      value,
      static_type_semantics<type>(),
      builtin_ops_of<type>(),
      meta::capabilities::static_capabilities_data<type>(),
      meta::capabilities::static_capabilities_size<type>(),
      false,
  };
}

template <typename T> [[nodiscard]] const type_record* static_type_record_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  static const type_record data = make_static_type_record<type>();
  return &data;
}

} // namespace caors::meta::detail

#endif
