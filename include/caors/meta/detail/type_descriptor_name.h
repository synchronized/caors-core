// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_DESCRIPTOR_NAME_H_
#define CAORS_META_DETAIL_TYPE_DESCRIPTOR_NAME_H_

#include "caors/core/string_name.h"
#include "caors/traits/container.h"
#include "caors/traits/type_name.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

namespace caors::meta::detail {

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

} // namespace caors::meta::detail

#endif
