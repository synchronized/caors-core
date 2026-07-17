// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_METHOD_H_
#define CAORS_META_METHOD_H_

#include "caors/export.h"
#include "caors/meta/metadata.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any_ref.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

namespace caors::meta {

namespace detail {
class method_set_data;
} // namespace detail

class method_set;
class instance;

/**
 * @brief 成员方法反射只读视图。
 *
 * `meta::method` 不拥有方法数据，只引用全局 `meta::registry`
 * 中的内部记录。调用时需要显式传入对象地址和 参数引用；参数类型不匹配、对象为空或 const 对象调用非
 * const 方法时会返回无效 `any`。
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
  [[nodiscard]] const meta::metadata_map& metadata() const noexcept;
  [[nodiscard]] const meta::metadata_value* find_metadata(meta::metadata_key key) const noexcept;

  /** @brief 在实例上调用成员方法；只读实例只能调用 const 成员函数。 */
  [[nodiscard]] any invoke(const instance& object, std::span<const any_ref> arguments = {}) const;

  /** @brief 调用全局函数。成员方法会失败并返回无效 `any`。 */
  [[nodiscard]] any invoke(std::span<const any_ref> arguments = {}) const;

  /** @brief 按指定类型调用方法并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(const instance& object, std::span<const any_ref> arguments = {}) const {
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

  /** @brief 按指定类型调用全局函数并读取返回值。 */
  template <typename Return>
  [[nodiscard]] std::optional<std::remove_cv_t<std::remove_reference_t<Return>>>
  try_invoke(std::span<const any_ref> arguments = {}) const {
    using return_type = std::remove_cv_t<std::remove_reference_t<Return>>;
    static_assert(std::is_copy_constructible_v<return_type>,
                  "Reflected method return values must be copy constructible");

    any result = invoke(arguments);
    const return_type* value = result.template try_get<return_type>();
    if (value == nullptr) {
      return std::nullopt;
    }

    return *value;
  }

private:
  friend class method_set;
  friend class module_view;
  friend class type;

  explicit method(const void* reflected_method) noexcept;
  [[nodiscard]] const void* info() const noexcept;

  const void* method_ = nullptr;
};

/**
 * @brief 同名成员方法重载集合只读视图。
 *
 * `meta::method_set` 不拥有方法数据。它表示同一个方法名下的一组具体重载；调用时会根据
 * `any_ref` 参数匹配唯一重载，匹配失败或存在多个匹配时返回无效 `any`。
 */
class CAORS_API method_set final {
public:
  method_set() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] std::size_t overload_count() const noexcept;
  [[nodiscard]] method overload_at(std::size_t index) const;
  [[nodiscard]] method single() const;
  [[nodiscard]] method resolve(std::span<const any_ref> arguments = {}) const;

  /** @brief 在实例上调用唯一匹配的成员方法重载。 */
  [[nodiscard]] any invoke(const instance& object, std::span<const any_ref> arguments = {}) const;

  /** @brief 调用唯一匹配的全局函数重载。 */
  [[nodiscard]] any invoke(std::span<const any_ref> arguments = {}) const;

private:
  friend class module_view;
  friend class type;

  explicit method_set(const void* reflected_method_set) noexcept;
  [[nodiscard]] const void* info() const noexcept;

  const void* method_set_ = nullptr;
};

} // namespace caors::meta

#endif
