// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_REFLECTION_METHOD_H_
#define CAORS_REFLECTION_METHOD_H_

#include "caors/export.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::reflection {

/**
 * @brief 成员方法反射只读视图。
 *
 * `reflection::method` 不拥有方法数据，只引用全局反射注册表中的内部记录。调用时需要显式传入对象地址
 * 和参数引用；参数类型不匹配、对象为空或 const 对象调用非 const 方法时会返回无效 `any`。
 */
class CAORS_API method final {
public:
  method() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] meta::type get_owner_type() const noexcept;
  [[nodiscard]] meta::type get_return_type() const noexcept;
  [[nodiscard]] std::size_t parameter_count() const noexcept;
  [[nodiscard]] std::optional<meta::type> get_parameter_type(std::size_t index) const noexcept;
  [[nodiscard]] bool const_method() const noexcept;

  /** @brief 在可写对象上调用方法。 */
  [[nodiscard]] any invoke(void* object, std::span<const any_ref> arguments = {}) const;

  /** @brief 在只读对象上调用方法；非 const 方法会失败并返回无效 `any`。 */
  [[nodiscard]] any invoke(const void* object, std::span<const any_ref> arguments = {}) const;

  /** @brief 按指定类型调用方法并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(void* object, std::span<const any_ref> arguments = {}) const {
    using return_type = std::remove_cv_t<std::remove_reference_t<Return>>;
    static_assert(std::is_copy_constructible_v<return_type>,
                  "Reflected method return values must be copy constructible");

    any result = invoke(object, arguments);
    const return_type* value = result.template try_get<return_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

  /** @brief 按指定类型在只读对象上调用方法并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(const void* object, std::span<const any_ref> arguments = {}) const {
    using return_type = std::remove_cv_t<std::remove_reference_t<Return>>;
    static_assert(std::is_copy_constructible_v<return_type>,
                  "Reflected method return values must be copy constructible");

    any result = invoke(object, arguments);
    const return_type* value = result.template try_get<return_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

private:
  friend class type;

  explicit method(const void* reflected_method) noexcept;
  [[nodiscard]] const void* info() const noexcept;

  const void* method_ = nullptr;
};

} // namespace caors::reflection

#endif
