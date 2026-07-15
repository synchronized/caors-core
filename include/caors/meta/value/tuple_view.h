// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_TUPLE_VIEW_H_
#define CAORS_META_VALUE_TUPLE_VIEW_H_

#include "caors/meta/value/fwd.h"

namespace caors::meta {

/**
 * @brief tuple-like 运行期视图。
 *
 * `tuple_view` 不拥有 tuple，只引用 `any` 或 `any_ref` 指向的 tuple-like
 * 对象。元素类型可以逐个查询， 元素值通过下标暴露为 `any_ref`。
 */
class tuple_view final {
public:
  tuple_view() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] bool writable() const noexcept { return valid() && writable_; }

  /** @brief 查询 tuple 元素数量。 */
  [[nodiscard]] std::size_t size() const noexcept;

  /** @brief 查询指定元素类型。 */
  [[nodiscard]] std::optional<meta::type> get_element_type(std::size_t index) const noexcept;

  /** @brief 获取指定元素引用。 */
  [[nodiscard]] any_ref element_ref(std::size_t index) const;

  /** @brief 写入指定元素。 */
  bool set_element(std::size_t index, const any& value) const;

private:
  friend class any;
  friend class any_ref;
  friend tuple_view make_tuple_view(any_ref source) noexcept;

  tuple_view(meta::type type, const void* storage, bool writable) noexcept;

  [[nodiscard]] const meta::tuple_ops* data() const noexcept;

  meta::type type_{};
  const void* storage_ = nullptr;
  bool writable_ = false;
};

} // namespace caors::meta

#endif
