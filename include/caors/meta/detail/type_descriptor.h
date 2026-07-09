// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/capability.h"
#include "caors/meta/detail/builtin_ops_model.h"
#include "caors/meta/type.h"
#include "caors/meta/type_semantics.h"
#include "caors/traits/type_name.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace caors::meta::detail {

struct type_descriptor;

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept;
[[nodiscard]] CAORS_API error ensure_type_descriptor_registered(const type_descriptor& descriptor);

/** @brief C++ 类型布局信息。 */
struct type_layout {
  std::size_t size = 0;
  std::size_t alignment = 0;

  [[nodiscard]] bool valid() const noexcept { return size > 0 && alignment > 0; }
};

/** @brief C++ 值的类型擦除操作函数表。 */
struct value_ops {
  value_default_construct_fn default_construct = nullptr;
  value_clone_fn clone = nullptr;
  value_destroy_fn destroy = nullptr;
  value_assign_fn assign = nullptr;
  value_resource_construct_fn resource_construct = nullptr;
  value_resource_destroy_fn resource_destroy = nullptr;
};

/** @brief C++ 值的编译期特征摘要。 */
struct value_traits {
  bool trivially_copyable = false;
  bool trivially_destructible = false;
  bool trivially_default_constructible = false;
};

/** @brief C++ 类型的运行期语义摘要。 */
struct type_semantics {
  type_kind kind = type_kind::value;
  meta::wrapper_semantics wrapper = meta::wrapper_semantics::none;
  bool indirect = false;
  bool nullable = false;
};

[[nodiscard]] inline bool
builtin_type_capability_valid(const builtin_type_capability& capability) noexcept {
  return std::visit(
      [](auto value) noexcept {
        using value_type = std::remove_cv_t<std::remove_reference_t<decltype(value)>>;
        if constexpr (std::is_same_v<value_type, std::monostate>) {
          return false;
        } else {
          return value != nullptr && value->valid();
        }
      },
      capability);
}

/** @brief meta 内部类型描述符，保存布局、值操作和运行期视图能力。 */
struct type_descriptor {
  type_id type{};
  string_name name{};
  type_layout layout{};
  value_ops ops{};
  value_traits traits{};
  type_semantics semantics{};
  builtin_type_capability builtin_capability{};
  const capability_entry* static_capabilities = nullptr;
  std::size_t static_capability_count = 0;

  [[nodiscard]] bool valid() const noexcept { return type && layout.valid(); }
};

template <typename T> inline constexpr bool always_false_v = false;

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor& descriptor) noexcept {
  if constexpr (std::is_same_v<Capability, enum_ops> || std::is_same_v<Capability, indirect_ops> ||
                std::is_same_v<Capability, sequence_ops> || std::is_same_v<Capability, set_ops> ||
                std::is_same_v<Capability, associative_ops> ||
                std::is_same_v<Capability, pair_ops> || std::is_same_v<Capability, tuple_ops> ||
                std::is_same_v<Capability, variant_ops>) {
    const auto* value = std::get_if<const Capability*>(&descriptor.builtin_capability);
    return value != nullptr ? *value : nullptr;
  } else {
    static_assert(always_false_v<Capability>, "Unsupported CAORS type capability");
  }
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor_capability<Capability>(*descriptor) : nullptr;
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_static_capability(const type_descriptor& descriptor) noexcept {
  const capability_key key = capability_key_of<Capability>();
  for (std::size_t index = 0; index < descriptor.static_capability_count; ++index) {
    const capability_entry& entry = descriptor.static_capabilities[index];
    if (entry.key == key && entry.data != nullptr) {
      return static_cast<const Capability*>(entry.data);
    }
  }

  return nullptr;
}

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_static_capability(const type_descriptor* descriptor) noexcept {
  return descriptor != nullptr ? descriptor_static_capability<Capability>(*descriptor) : nullptr;
}

[[nodiscard]] inline type_kind descriptor_kind(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.kind;
}

[[nodiscard]] inline meta::wrapper_semantics
descriptor_wrapper_semantics(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.wrapper;
}

[[nodiscard]] inline bool descriptor_indirect(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.indirect;
}

[[nodiscard]] inline bool descriptor_nullable(const type_descriptor& descriptor) noexcept {
  return descriptor.semantics.nullable;
}

[[nodiscard]] inline type_id descriptor_value_type(const type_descriptor& descriptor) noexcept {
  switch (descriptor_kind(descriptor)) {
  case type_kind::value:
  case type_kind::enumeration:
    return descriptor.type;
  case type_kind::raw_pointer:
  case type_kind::wrapper: {
    const auto* indirect = descriptor_capability<indirect_ops>(descriptor);
    return indirect != nullptr && indirect->value_descriptor != nullptr
               ? indirect->value_descriptor->type
               : type_id{};
  }
  case type_kind::sequence: {
    const auto* sequence = descriptor_capability<sequence_ops>(descriptor);
    return sequence != nullptr ? sequence->value_type : type_id{};
  }
  case type_kind::set: {
    const auto* set = descriptor_capability<set_ops>(descriptor);
    return set != nullptr ? set->value_type : type_id{};
  }
  case type_kind::associative: {
    const auto* associative = descriptor_capability<associative_ops>(descriptor);
    return associative != nullptr ? associative->value_type : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_ops>(descriptor);
    return pair != nullptr ? pair->value_type : type_id{};
  }
  case type_kind::tuple:
  case type_kind::variant:
    return {};
  }

  return {};
}

[[nodiscard]] inline type_id descriptor_key_type(const type_descriptor& descriptor) noexcept {
  switch (descriptor_kind(descriptor)) {
  case type_kind::associative: {
    const auto* associative = descriptor_capability<associative_ops>(descriptor);
    return associative != nullptr ? associative->key_type : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_ops>(descriptor);
    return pair != nullptr ? pair->key_type : type_id{};
  }
  case type_kind::value:
  case type_kind::enumeration:
  case type_kind::raw_pointer:
  case type_kind::wrapper:
  case type_kind::sequence:
  case type_kind::set:
  case type_kind::tuple:
  case type_kind::variant:
    return {};
  }

  return {};
}

template <typename T> [[nodiscard]] type_id descriptor_type_id() noexcept {
  return type_id_of_cpp_type<std::remove_cv_t<std::remove_reference_t<T>>>();
}

template <typename T> [[nodiscard]] std::string descriptor_type_name_text();

template <typename T> [[nodiscard]] std::string descriptor_sequence_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::sequence_traits<type>::element_type;

  std::string text{traits::sequence_traits<type>::type_name_prefix()};
  std::string element = descriptor_type_name_text<element_type>();
  if (text.empty() || element.empty()) {
    return {};
  }

  text += "<";
  text += element;
  if constexpr (requires { traits::sequence_traits<type>::fixed_size; }) {
    text += ",";
    text += std::to_string(traits::sequence_traits<type>::fixed_size);
  }
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string descriptor_set_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::set_traits<type>::element_type;

  std::string text{traits::set_traits<type>::type_name_prefix()};
  std::string element = descriptor_type_name_text<element_type>();
  if (text.empty() || element.empty()) {
    return {};
  }

  text += "<";
  text += element;
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string descriptor_associative_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using key_type = typename traits::associative_traits<type>::key_type;
  using mapped_type = typename traits::associative_traits<type>::mapped_type;

  std::string text{traits::associative_traits<type>::type_name_prefix()};
  std::string key = descriptor_type_name_text<key_type>();
  std::string mapped = descriptor_type_name_text<mapped_type>();
  if (text.empty() || key.empty() || mapped.empty()) {
    return {};
  }

  text += "<";
  text += key;
  text += ",";
  text += mapped;
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string descriptor_pair_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

  std::string text{traits::pair_traits<type>::type_name_prefix()};
  std::string first = descriptor_type_name_text<first_type>();
  std::string second = descriptor_type_name_text<second_type>();
  if (text.empty() || first.empty() || second.empty()) {
    return {};
  }

  text += "<";
  text += first;
  text += ",";
  text += second;
  text += ">";
  return text;
}

template <typename Tuple, std::size_t... Indices>
[[nodiscard]] bool append_tuple_type_names(std::string& text, std::index_sequence<Indices...>) {
  using tuple_type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  bool ok = true;
  bool first = true;
  (void)(([&] {
           using element_type =
               typename traits::tuple_traits<tuple_type>::template element_type<Indices>;
           std::string name = descriptor_type_name_text<element_type>();
           if (name.empty()) {
             ok = false;
             return;
           }
           if (!first) {
             text += ",";
           }
           text += name;
           first = false;
         }()),
         ...);
  return ok;
}

template <typename T> [[nodiscard]] std::string descriptor_tuple_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  std::string text{traits::tuple_traits<type>::type_name_prefix()};
  if (text.empty()) {
    return {};
  }

  text += "<";
  if (!append_tuple_type_names<type>(
          text, std::make_index_sequence<traits::tuple_traits<type>::size>{})) {
    return {};
  }
  text += ">";
  return text;
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] bool append_variant_type_names(std::string& text, std::index_sequence<Indices...>) {
  using variant_type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  bool ok = true;
  bool first = true;
  (void)(([&] {
           using alternative_type =
               typename traits::variant_traits<variant_type>::template alternative_type<Indices>;
           std::string name = descriptor_type_name_text<alternative_type>();
           if (name.empty()) {
             ok = false;
             return;
           }
           if (!first) {
             text += ",";
           }
           text += name;
           first = false;
         }()),
         ...);
  return ok;
}

template <typename T> [[nodiscard]] std::string descriptor_variant_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  std::string text{traits::variant_traits<type>::type_name_prefix()};
  if (text.empty()) {
    return {};
  }

  text += "<";
  if (!append_variant_type_names<type>(
          text, std::make_index_sequence<traits::variant_traits<type>::size>{})) {
    return {};
  }
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string descriptor_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (traits::has_type_name_v<type>) {
    return std::string{traits::type_name<type>::value};
  } else if constexpr (std::is_pointer_v<type>) {
    std::string text = descriptor_type_name_text<std::remove_cv_t<std::remove_pointer_t<type>>>();
    if (text.empty()) {
      return {};
    }
    text += "*";
    return text;
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    return descriptor_sequence_type_name_text<type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    return descriptor_set_type_name_text<type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return descriptor_associative_type_name_text<type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return descriptor_pair_type_name_text<type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return descriptor_tuple_type_name_text<type>();
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return descriptor_variant_type_name_text<type>();
  } else {
    return {};
  }
}

template <typename T> [[nodiscard]] string_name descriptor_type_name() {
  std::string text = descriptor_type_name_text<T>();
  return text.empty() ? string_name{} : string_name{text};
}

template <typename T> void fill_descriptor_semantics(type_descriptor& descriptor) {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_cv_t<std::remove_pointer_t<type>>;

    static_assert(!is_nested_type_indirection_v<value_type>,
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

    static_assert(!is_nested_type_indirection_v<value_type>,
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

template <typename T> struct descriptor_holder {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  inline static const type_descriptor value = [] {
    type_descriptor descriptor{};
    descriptor.type = descriptor_type_id<type>();
    descriptor.name = descriptor_type_name<type>();
    descriptor.layout = type_layout{sizeof(type), alignof(type)};
    descriptor.ops = value_ops{
        default_construct_fn_of<type>(),
        value_clone_fn_of<type>(),
        value_destroy_fn_of<type>(),
        value_assign_fn_of<type>(),
        resource_default_construct_fn_of<type>(),
        resource_destroy_fn_of<type>(),
    };
    descriptor.traits = value_traits{
        std::is_trivially_copyable_v<type>,
        std::is_trivially_destructible_v<type>,
        std::is_trivially_default_constructible_v<type>,
    };
    fill_descriptor_semantics<type>(descriptor);
    descriptor.builtin_capability = builtin_type_capability_of<type>();
    descriptor.static_capabilities = meta::capabilities::static_capabilities_data<type>();
    descriptor.static_capability_count = meta::capabilities::static_capabilities_size<type>();
    return descriptor;
  }();
};

template <typename T> [[nodiscard]] const type_descriptor* descriptor_ptr_of() noexcept {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  return &descriptor_holder<type>::value;
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
