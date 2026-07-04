// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_INDIRECT_VIEW_H_
#define CAORS_VALUE_INDIRECT_VIEW_H_

#include "caors/value/any_value.h"

namespace caors::value {

/**
 * @brief 指针和 wrapper 的内部值视图。
 *
 * `indirect_view` 不拥有 wrapper 或内部值，只引用 `any` / `any_ref` 指向的 wrapper 对象。
 * 可写视图才允许重置 wrapper 或返回可写内部值引用。
 */
class indirect_view final {
public:
  indirect_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }
  [[nodiscard]] type_id value_type() const noexcept;
  [[nodiscard]] string_name value_type_name() const noexcept;

  /** @brief 判断 wrapper 当前是否持有内部值。 */
  [[nodiscard]] bool has_value() const noexcept;

  /** @brief 获取内部值引用。 */
  [[nodiscard]] any_ref value_ref() const;

  /** @brief 重置 wrapper 或裸指针为空。 */
  bool reset() const;

  /** @brief 确保 wrapper 持有内部值。裸指针不会自动分配内存。 */
  bool ensure_value() const;

private:
  friend class any;
  friend class any_ref;

  indirect_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                bool writable) noexcept;

  [[nodiscard]] const meta::detail::indirect_data* data() const noexcept;

  const meta::detail::type_descriptor* descriptor_ = nullptr;
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::value

#endif
