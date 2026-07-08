// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_VARIANT_VIEW_H_
#define CAORS_VALUE_VARIANT_VIEW_H_

#include "caors/value/fwd.h"

namespace caors::value {

/**
 * @brief variant-like 运行期视图。
 *
 * `variant_view` 不拥有 variant，只引用 `any` 或 `any_ref` 指向的 variant-like 对象。视图使用
 * 当前候选下标定位真实值，序列化层会按 `[index, value]` 形式保存。
 */
class variant_view final {
public:
  variant_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询候选类型数量。 */
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 查询当前候选下标。variant 处于 valueless 状态时返回空。 */
  [[nodiscard]] std::optional<std::size_t> index() const noexcept;

  /** @brief 查询指定候选类型。 */
  [[nodiscard]] std::optional<meta::type>
  get_alternative_type(std::size_t alternative) const noexcept;
  [[nodiscard]] std::optional<type_id> alternative_type(std::size_t alternative) const noexcept;

  /** @brief 查询指定候选类型名。 */
  [[nodiscard]] string_name alternative_type_name(std::size_t alternative) const noexcept;

  /** @brief 查询当前值类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;
  [[nodiscard]] type_id value_type() const noexcept;

  /** @brief 查询当前值类型名。 */
  [[nodiscard]] string_name value_type_name() const noexcept;

  /** @brief 获取当前候选值引用。 */
  [[nodiscard]] any_ref value_ref() const;

  /** @brief 切换当前候选。已处于目标候选时直接成功。 */
  bool emplace(std::size_t alternative) const;

  /** @brief 使用指定资源切换当前候选。已处于目标候选时直接成功。 */
  bool emplace(std::size_t alternative, memory::memory_resource* resource) const;

  /** @brief 写入当前候选值。 */
  bool set_value(const any& value) const;

private:
  friend class any;
  friend class any_ref;

  variant_view(const meta::detail::type_descriptor* descriptor, const void* storage,
               bool writable) noexcept;

  [[nodiscard]] const meta::detail::variant_data* data() const noexcept;

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
