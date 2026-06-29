// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_TYPE_NAME_H_
#define CAORS_META_TYPE_NAME_H_

#include "caors/meta/type_id.h"

#include <string>
#include <string_view>

namespace caors {

/**
 * @brief C++ 类型到 CAORS 类型名的默认映射。
 *
 * 用户类型可以通过特化 `type_name<T>` 提供默认名称。没有默认名称的类型需要先显式注册到
 * `meta_registry`，或者在反射注册时手动传入字段类型名。
 */
template <typename T> struct type_name;

template <> struct type_name<bool> {
  inline static constexpr std::string_view value = "bool";
};

template <> struct type_name<char> {
  inline static constexpr std::string_view value = "char";
};

template <> struct type_name<signed char> {
  inline static constexpr std::string_view value = "signed_char";
};

template <> struct type_name<unsigned char> {
  inline static constexpr std::string_view value = "unsigned_char";
};

template <> struct type_name<short> {
  inline static constexpr std::string_view value = "short";
};

template <> struct type_name<unsigned short> {
  inline static constexpr std::string_view value = "unsigned_short";
};

template <> struct type_name<int> {
  inline static constexpr std::string_view value = "int";
};

template <> struct type_name<unsigned int> {
  inline static constexpr std::string_view value = "unsigned_int";
};

template <> struct type_name<long> {
  inline static constexpr std::string_view value = "long";
};

template <> struct type_name<unsigned long> {
  inline static constexpr std::string_view value = "unsigned_long";
};

template <> struct type_name<long long> {
  inline static constexpr std::string_view value = "long_long";
};

template <> struct type_name<unsigned long long> {
  inline static constexpr std::string_view value = "unsigned_long_long";
};

template <> struct type_name<float> {
  inline static constexpr std::string_view value = "float";
};

template <> struct type_name<double> {
  inline static constexpr std::string_view value = "double";
};

template <> struct type_name<long double> {
  inline static constexpr std::string_view value = "long_double";
};

template <> struct type_name<std::string> {
  inline static constexpr std::string_view value = "std.string";
};

template <> struct type_name<std::string_view> {
  inline static constexpr std::string_view value = "std.string_view";
};

template <> struct type_name<guid> {
  inline static constexpr std::string_view value = "caors.guid";
};

template <> struct type_name<type_guid> {
  inline static constexpr std::string_view value = "caors.type_guid";
};

} // namespace caors

#endif
