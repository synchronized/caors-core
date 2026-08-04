// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_STATIC_TYPE_NAME_H_
#define CAORS_META_DETAIL_STATIC_TYPE_NAME_H_

#include "caors/core/string_name.h"
#include "caors/traits/container.h"
#include "caors/traits/type_identity.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

template <typename T> [[nodiscard]] std::string static_type_name_text();

template <typename T> [[nodiscard]] std::string sequence_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::sequence_traits<type>::element_type;

  std::string text{traits::sequence_traits<type>::type_name_prefix()};
  std::string element = static_type_name_text<element_type>();
  if (text.empty() || element.empty()) {
    return {};
  }

  text += "<";
  text += element;
  if constexpr (requires { traits::sequence_traits<type>::fixed_size; }) {
    text += ",";
    text += std::to_string(traits::sequence_traits<type>::fixed_size);
  } else if constexpr (requires { traits::sequence_traits<type>::inline_capacity; }) {
    text += ",";
    text += std::to_string(traits::sequence_traits<type>::inline_capacity);
  }
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string set_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using element_type = typename traits::set_traits<type>::element_type;

  std::string text{traits::set_traits<type>::type_name_prefix()};
  std::string element = static_type_name_text<element_type>();
  if (text.empty() || element.empty()) {
    return {};
  }

  text += "<";
  text += element;
  text += ">";
  return text;
}

template <typename T> [[nodiscard]] std::string associative_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using key_type = typename traits::associative_traits<type>::key_type;
  using mapped_type = typename traits::associative_traits<type>::mapped_type;

  std::string text{traits::associative_traits<type>::type_name_prefix()};
  std::string key = static_type_name_text<key_type>();
  std::string mapped = static_type_name_text<mapped_type>();
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

template <typename T> [[nodiscard]] std::string pair_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  using first_type = typename traits::pair_traits<type>::first_type;
  using second_type = typename traits::pair_traits<type>::second_type;

  std::string text{traits::pair_traits<type>::type_name_prefix()};
  std::string first = static_type_name_text<first_type>();
  std::string second = static_type_name_text<second_type>();
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
           std::string name = static_type_name_text<element_type>();
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

template <typename T> [[nodiscard]] std::string tuple_type_name_text() {
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
           std::string name = static_type_name_text<alternative_type>();
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

template <typename T> [[nodiscard]] std::string variant_type_name_text() {
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

template <typename T> [[nodiscard]] std::string static_type_name_text() {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;

  const std::string_view declared_name = traits::type_identity<type>::name();
  if (!declared_name.empty()) {
    return std::string{declared_name};
  }

  if constexpr (std::is_pointer_v<type>) {
    std::string text = static_type_name_text<std::remove_cv_t<std::remove_pointer_t<type>>>();
    if (text.empty()) {
      return {};
    }
    text += "*";
    return text;
  } else if constexpr (traits::sequence_traits<type>::is_sequence) {
    return sequence_type_name_text<type>();
  } else if constexpr (traits::set_traits<type>::is_set) {
    return set_type_name_text<type>();
  } else if constexpr (traits::associative_traits<type>::is_associative) {
    return associative_type_name_text<type>();
  } else if constexpr (traits::pair_traits<type>::is_pair) {
    return pair_type_name_text<type>();
  } else if constexpr (traits::tuple_traits<type>::is_tuple) {
    return tuple_type_name_text<type>();
  } else if constexpr (traits::variant_traits<type>::is_variant) {
    return variant_type_name_text<type>();
  } else {
    return {};
  }
}

template <typename T> [[nodiscard]] string_name static_type_name() {
  std::string text = static_type_name_text<T>();
  return text.empty() ? string_name{} : string_name{text};
}

} // namespace caors::meta::detail

#endif
