// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_H_

#include "caors/meta/capability.h"
#include "caors/meta/detail/container.h"
#include "caors/meta/detail/container_iterators.h"
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

/** @brief 类型的内置互斥运行期能力。 */
using builtin_type_capability =
    std::variant<std::monostate, const enum_data*, const indirect_data*, const sequence_data*,
                 const set_data*, const associative_data*, const pair_data*, const tuple_data*,
                 const variant_data*>;

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

  [[nodiscard]] bool has_view_capability() const noexcept {
    if (builtin_type_capability_valid(builtin_capability)) {
      return true;
    }

    for (std::size_t index = 0; index < static_capability_count; ++index) {
      if (static_capabilities != nullptr && static_capabilities[index].valid()) {
        return true;
      }
    }

    return false;
  }
};

template <typename T> inline constexpr bool always_false_v = false;

template <typename Capability>
[[nodiscard]] inline const Capability*
descriptor_capability(const type_descriptor& descriptor) noexcept {
  if constexpr (std::is_same_v<Capability, enum_data> ||
                std::is_same_v<Capability, indirect_data> ||
                std::is_same_v<Capability, sequence_data> || std::is_same_v<Capability, set_data> ||
                std::is_same_v<Capability, associative_data> ||
                std::is_same_v<Capability, pair_data> || std::is_same_v<Capability, tuple_data> ||
                std::is_same_v<Capability, variant_data>) {
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
    const auto* indirect = descriptor_capability<indirect_data>(descriptor);
    return indirect != nullptr && indirect->value_descriptor != nullptr
               ? indirect->value_descriptor->type
               : type_id{};
  }
  case type_kind::sequence: {
    const auto* sequence = descriptor_capability<sequence_data>(descriptor);
    return sequence != nullptr ? sequence->value_type : type_id{};
  }
  case type_kind::set: {
    const auto* set = descriptor_capability<set_data>(descriptor);
    return set != nullptr ? set->value_type : type_id{};
  }
  case type_kind::associative: {
    const auto* associative = descriptor_capability<associative_data>(descriptor);
    return associative != nullptr ? associative->value_type : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_data>(descriptor);
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
    const auto* associative = descriptor_capability<associative_data>(descriptor);
    return associative != nullptr ? associative->key_type : type_id{};
  }
  case type_kind::pair: {
    const auto* pair = descriptor_capability<pair_data>(descriptor);
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

template <typename T> [[nodiscard]] bool append_descriptor_type_name(std::string& text) {
  std::string name = descriptor_type_name_text<T>();
  if (name.empty()) {
    return false;
  }

  text += name;
  return true;
}

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

template <typename T> [[nodiscard]] const enum_data* enum_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_enum_v<type>) {
    static const enum_data data = make_enum_data<type>();
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const indirect_data* indirect_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (std::is_pointer_v<type>) {
    using value_type = std::remove_pointer_t<type>;
    static const indirect_data data = [] {
      indirect_data result = make_indirect_data<type>();
      result.value_descriptor = descriptor_ptr_of<std::remove_cv_t<value_type>>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    using value_type = std::remove_cv_t<typename traits::wrapper_traits<type>::wrapped_type>;
    static const indirect_data data = [] {
      indirect_data result{};
      if constexpr (traits::wrapper_traits<type>::semantics != wrapper_semantics::unsupported) {
        result = make_indirect_data<type>();
      }
      result.value_descriptor = descriptor_ptr_of<value_type>();
      return result;
    }();

    return data.value_descriptor != nullptr ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const sequence_data* sequence_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::sequence_traits<type>::is_sequence) {
    using element_type = typename traits::sequence_traits<type>::element_type;
    static const sequence_data data = [] {
      sequence_data result{
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

      if constexpr (requires { traits::sequence_traits<type>::fixed_size; }) {
        result.fixed_size = traits::sequence_traits<type>::fixed_size;
      }

      result.value_descriptor = descriptor_ptr_of<element_type>();
      return result;
    }();

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const set_data* set_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::set_traits<type>::is_set) {
    using element_type = typename traits::set_traits<type>::element_type;
    static const set_data data = [] {
      set_data result{
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

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const associative_data* associative_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::associative_traits<type>::is_associative) {
    using key_type = typename traits::associative_traits<type>::key_type;
    using mapped_type = typename traits::associative_traits<type>::mapped_type;
    static const associative_data data = [] {
      associative_data result{
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

            return traits::associative_traits<type>::insert(
                *static_cast<type*>(storage), *static_cast<const key_type*>(key),
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

    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] const pair_data* pair_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::pair_traits<type>::is_pair) {
    using first_type = typename traits::pair_traits<type>::first_type;
    using second_type = typename traits::pair_traits<type>::second_type;
    static const pair_data data = [] {
      pair_data result{
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
[[nodiscard]] tuple_element_data make_tuple_element_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Tuple>>;
  using element_type = typename traits::tuple_traits<type>::template element_type<Index>;

  return tuple_element_data{
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
[[nodiscard]] tuple_data make_tuple_descriptor_data(std::index_sequence<Indices...>) {
  tuple_data data{};
  data.elements.reserve(sizeof...(Indices));
  (data.elements.push_back(make_tuple_element_descriptor_data<Tuple, Indices>()), ...);
  return data;
}

template <typename T> [[nodiscard]] const tuple_data* tuple_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::tuple_traits<type>::is_tuple) {
    static const tuple_data data = make_tuple_descriptor_data<type>(
        std::make_index_sequence<traits::tuple_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename Variant, std::size_t Index>
[[nodiscard]] variant_alternative_data make_variant_alternative_descriptor_data() {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;
  using alternative_type = typename traits::variant_traits<type>::template alternative_type<Index>;

  return variant_alternative_data{
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
}

template <typename Variant, std::size_t... Indices>
[[nodiscard]] variant_data make_variant_descriptor_data(std::index_sequence<Indices...>) {
  using type = std::remove_cv_t<std::remove_reference_t<Variant>>;

  variant_data data{};
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

template <typename T> [[nodiscard]] const variant_data* variant_data_ptr_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (traits::variant_traits<type>::is_variant) {
    static const variant_data data = make_variant_descriptor_data<type>(
        std::make_index_sequence<traits::variant_traits<type>::size>{});
    return data.valid() ? &data : nullptr;
  } else {
    return nullptr;
  }
}

template <typename T> [[nodiscard]] builtin_type_capability builtin_type_capability_of() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  if constexpr (std::is_pointer_v<type>) {
    return indirect_data_ptr_of<type>();
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_data_ptr_of<type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    return set_data_ptr_of<type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return associative_data_ptr_of<type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_data_ptr_of<type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_data_ptr_of<type>();
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_data_ptr_of<type>();
  } else if constexpr (traits::wrapper_traits<type>::is_wrapper) {
    return indirect_data_ptr_of<type>();
  } else if constexpr (std::is_enum_v<type>) {
    return enum_data_ptr_of<type>();
  } else {
    return std::monostate{};
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
