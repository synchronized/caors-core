// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_RUNTIME_TYPE_H_
#define CAORS_RUNTIME_TYPE_H_

#include "caors/export.h"
#include "caors/memory/resource.h"
#include "caors/meta/type.h"
#include "caors/meta/value/any.h"
#include "caors/meta/value/any_ref.h"
#include "caors/reflection/type.h"

#include <cstddef>
#include <span>
#include <string_view>

namespace caors::runtime {

using field = reflection::field;
using method = reflection::method;

/**
 * @brief 运行时类型组合视图。
 *
 * `runtime::type` 是用户友好的 facade，不拥有任何类型数据。它以 `meta::type` 为主视图，
 * 并按需查询反射组件。缺少反射组件时，字段和方法查询返回空视图。
 */
class CAORS_API type final {
public:
  type() noexcept = default;
  explicit type(meta::type meta_type) noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  [[nodiscard]] meta::type meta() const noexcept;
  [[nodiscard]] reflection::type reflection() const noexcept;
  [[nodiscard]] bool reflectable() const noexcept;

  [[nodiscard]] type_id id() const noexcept;
  [[nodiscard]] string_name name() const noexcept;
  [[nodiscard]] type_guid guid() const noexcept;
  [[nodiscard]] type parent() const noexcept;
  [[nodiscard]] meta::type_kind kind() const noexcept;
  [[nodiscard]] meta::wrapper_semantics wrapper() const noexcept;
  [[nodiscard]] bool indirect() const noexcept;
  [[nodiscard]] bool nullable() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t alignment() const noexcept;

  /** @brief 查询该类型是否能通过 runtime 高层创建入口构造运行期值。 */
  [[nodiscard]] bool constructible() const noexcept;

  /** @brief 查询该类型是否能使用指定参数数量通过 runtime 高层创建入口构造运行期值。 */
  [[nodiscard]] bool constructible(std::size_t argument_count) const noexcept;

  /**
   * @brief 创建运行期值。
   *
   * 优先使用已注册的 `meta::factory_capability`，再尝试无参 `meta::constructor_capability`，
   * 最后回退到 descriptor 默认构造。
   */
  [[nodiscard]] any create(memory::memory_resource* resource = nullptr) const;

  /**
   * @brief 使用运行期参数列表创建运行期值。
   *
   * 空参数列表保持无参 `create()` 语义；非空参数列表只查询已注册的 constructor capability。
   */
  [[nodiscard]] any create(std::span<const meta::any_ref> arguments,
                           memory::memory_resource* resource = nullptr) const;

  /** @brief 查询 descriptor 默认构造能力。 */
  [[nodiscard]] bool value_constructible() const noexcept;

  /** @brief 使用 descriptor 默认构造创建运行期值，不查询 factory capability。 */
  [[nodiscard]] any create_value(memory::memory_resource* resource = nullptr) const;

  [[nodiscard]] std::size_t field_count() const noexcept;
  [[nodiscard]] field field_at(std::size_t index) const;
  [[nodiscard]] field find_field(string_name name) const;
  [[nodiscard]] field find_field(std::string_view name) const;

  [[nodiscard]] std::size_t method_count() const noexcept;
  [[nodiscard]] method method_at(std::size_t index) const;
  [[nodiscard]] method find_method(string_name name) const;
  [[nodiscard]] method find_method(std::string_view name) const;

private:
  meta::type meta_{};
};

} // namespace caors::runtime

#endif
