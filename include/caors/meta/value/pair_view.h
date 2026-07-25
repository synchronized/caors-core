// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_PAIR_VIEW_H_
#define CAORS_META_VALUE_PAIR_VIEW_H_

#include "caors/meta/value/fwd.h"

namespace caors::meta {

/**
 * @brief 二元组运行期视图。
 *
 * `pair_view` 不拥有二元组，只引用 `any` 或 `any_ref` 指向的 pair 对象。first 通常用于 key
 * 语义，second 作为统一 value 语义暴露。
 */
class CAORS_API pair_view final {
public:
  pair_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 first 类型。 */
  [[nodiscard]] meta::type get_first_type() const noexcept;

  /** @brief 查询 second 的统一 value 类型。 */
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 查询 second 类型，等价于 `get_value_type()`。 */
  [[nodiscard]] meta::type get_second_type() const noexcept { return get_value_type(); }

  /** @brief 获取 first 引用。 */
  [[nodiscard]] any_ref first_ref() const;

  /** @brief 获取 second 引用。 */
  [[nodiscard]] any_ref second_ref() const;

  /** @brief 获取 second 引用，等价于 `second_ref()`。 */
  [[nodiscard]] any_ref value_ref() const;

  /** @brief 写入 first。 */
  bool set_first(const any& value) const;

  /** @brief 写入 second。 */
  bool set_second(const any& value) const;

private:
  friend class any;
  friend class any_ref;
  friend CAORS_API pair_view make_pair_view(any_ref source) noexcept;

  pair_view(meta::type type, const void* storage, bool writable) noexcept;

  [[nodiscard]] const meta::pair_ops* data() const noexcept;

  meta::type type_{};
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::meta

#endif
