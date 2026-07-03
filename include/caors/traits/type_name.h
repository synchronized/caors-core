// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_TYPE_NAME_H_
#define CAORS_TRAITS_TYPE_NAME_H_

#include "caors/core/containers.h"
#include "caors/core/type_guid.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

namespace caors::traits {

/**
 * @brief C++ 类型到 CAORS 类型名的默认映射。
 *
 * 用户类型可以通过特化 `traits::type_name<T>` 提供默认名称。没有默认名称的类型需要先显式
 * 注册到 `meta::registry`，或者在反射注册时手动传入字段类型名。
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

template <> struct type_name<core::string> {
  inline static constexpr std::string_view value = "caors.string";
};

template <> struct type_name<std::string_view> {
  inline static constexpr std::string_view value = "std.string_view";
};

template <> struct type_name<std::filesystem::path> {
  inline static constexpr std::string_view value = "std.filesystem.path";
};

template <> struct type_name<std::chrono::nanoseconds> {
  inline static constexpr std::string_view value = "std.chrono.nanoseconds";
};

template <> struct type_name<std::chrono::microseconds> {
  inline static constexpr std::string_view value = "std.chrono.microseconds";
};

template <> struct type_name<std::chrono::milliseconds> {
  inline static constexpr std::string_view value = "std.chrono.milliseconds";
};

template <> struct type_name<std::chrono::seconds> {
  inline static constexpr std::string_view value = "std.chrono.seconds";
};

template <> struct type_name<std::chrono::minutes> {
  inline static constexpr std::string_view value = "std.chrono.minutes";
};

template <> struct type_name<std::chrono::hours> {
  inline static constexpr std::string_view value = "std.chrono.hours";
};

template <> struct type_name<std::chrono::system_clock::time_point> {
  inline static constexpr std::string_view value = "std.chrono.system_clock.time_point";
};

template <> struct type_name<core::guid> {
  inline static constexpr std::string_view value = "caors.guid";
};

template <> struct type_name<core::type_guid> {
  inline static constexpr std::string_view value = "caors.type_guid";
};

namespace detail {

template <typename T, typename = void> struct has_type_name : std::false_type {};

template <typename T>
struct has_type_name<
    T, std::void_t<decltype(type_name<std::remove_cv_t<std::remove_reference_t<T>>>::value)>>
    : std::true_type {};

} // namespace detail

template <typename T> inline constexpr bool has_type_name_v = detail::has_type_name<T>::value;

} // namespace caors::traits

#endif
