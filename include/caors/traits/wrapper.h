// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TRAITS_WRAPPER_H_
#define CAORS_TRAITS_WRAPPER_H_

namespace caors::traits {

/** @brief wrapper 接管外部构造值时可选的销毁回调上下文。 */
struct wrapper_set_context {
  using destroy_fn = void (*)(void* user_data, void* value) noexcept;

  void* user_data = nullptr;
  destroy_fn destroy = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept { return destroy != nullptr; }
};

/** @brief wrapper 类型的序列化语义。 */
enum class wrapper_semantics {
  none,
  inline_value,
  unsupported,
};

/** @brief C++ wrapper 类型到 meta 类型语义的映射。 */
template <typename T> struct wrapper_traits {
  static constexpr bool is_wrapper = false;
  static constexpr bool nullable = false;
  static constexpr wrapper_semantics semantics = wrapper_semantics::none;
};

/** @brief wrapper 类型的内部值访问适配。 */
template <typename T> struct wrapper_access {
  using wrapped_type = void;

  [[nodiscard]] static const void* value(const T&) noexcept { return nullptr; }
  [[nodiscard]] static void* mutable_value(T&) noexcept { return nullptr; }
  [[nodiscard]] static bool has_value(const T&) noexcept { return false; }
  static bool reset(T&) noexcept { return false; }
  static bool ensure_value(T&) { return false; }
};

} // namespace caors::traits

#endif
