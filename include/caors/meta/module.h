// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_MODULE_H_
#define CAORS_META_MODULE_H_

#include "caors/export.h"
#include "caors/meta/detail/callable_signature.h"
#include "caors/meta/method.h"
#include "caors/meta/property.h"

#include <string_view>
#include <type_traits>
#include <utility>

namespace caors::meta {

class module_builder;

namespace detail {
class method_record;
class module_access;
class module_record;
class property_record;
} // namespace detail

/** @brief 全局 module 只读视图，用于查询全局属性和全局函数。 */
class CAORS_API module_view final {
public:
  module_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] std::size_t property_count() const noexcept;
  [[nodiscard]] property property_at(std::size_t index) const;
  [[nodiscard]] property find_property(string_name name) const;
  [[nodiscard]] property find_property(std::string_view name) const;
  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method_set method_at(std::size_t index) const;
  [[nodiscard]] method_set find_method(string_name name) const;
  [[nodiscard]] method_set find_method(std::string_view name) const;

private:
  friend class detail::module_access;
  explicit module_view(const detail::module_record* data) noexcept;

  const detail::module_record* data_ = nullptr;
};

/**
 * @brief module 注册构建器，用于即时注册全局属性和全局函数。
 *
 * 与持有 pending record 的类型注册构建器不同，每次 `property()` 或 `function()` 调用都会立即
 * 写入 module；该构建器没有最终 `commit()` 步骤。
 */
class CAORS_API module_builder final {
public:
  explicit module_builder(std::string_view name);

  template <typename Property> module_builder& property(std::string_view name, Property* value);

  template <typename Return, typename... Arguments>
  module_builder& function(std::string_view name, Return (*function)(Arguments...));

  template <typename Callable>
    requires(!std::is_pointer_v<std::decay_t<Callable>> &&
             requires { typename detail::callable_signature_t<std::decay_t<Callable>>; })
  module_builder& function(std::string_view name, Callable&& callable);

  [[nodiscard]] module_view get() const noexcept;
  [[nodiscard]] error result() const noexcept { return status_; }
  [[nodiscard]] bool succeeded() const noexcept { return !status_; }

private:
  [[nodiscard]] error add_property(detail::property_record property);
  [[nodiscard]] error add_method(detail::method_record method);
  void refresh() noexcept;

  string_name name_{};
  detail::module_record* module_ = nullptr;
  error status_{};
};

/** @brief 获取或创建指定 module 的注册构建器。 */
[[nodiscard]] CAORS_API module_builder module(std::string_view name);

/** @brief 查找指定 module。 */
[[nodiscard]] CAORS_API module_view find_module(std::string_view name);
[[nodiscard]] CAORS_API module_view find_module(string_name name);

} // namespace caors::meta

#include "caors/meta/detail/module_templates.h"

#endif
