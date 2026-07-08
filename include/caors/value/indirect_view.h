// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_VALUE_INDIRECT_VIEW_H_
#define CAORS_VALUE_INDIRECT_VIEW_H_

#include "caors/memory/resource.h"
#include "caors/traits/wrapper.h"
#include "caors/value/fwd.h"

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
  [[nodiscard]] meta::type get_value_type() const noexcept;

  /** @brief 判断 wrapper 当前是否持有内部值。 */
  [[nodiscard]] bool has_value() const noexcept;

  /** @brief 获取内部值引用。 */
  [[nodiscard]] any_ref value_ref() const;

  /** @brief 获取内部值的真实运行期类型。普通非多态值返回声明的 value 类型。 */
  [[nodiscard]] meta::type get_dynamic_value_type() const noexcept;

  /** @brief 按真实运行期类型获取内部值引用。 */
  [[nodiscard]] any_ref dynamic_value_ref() const;

  /** @brief 判断该 wrapper 是否能接管一个按真实类型构造出的内部值。 */
  [[nodiscard]] bool can_set_value() const noexcept;

  /** @brief 判断该 wrapper 是否能接管需要自定义销毁回调的内部值。 */
  [[nodiscard]] bool can_set_value_with_context() const noexcept;

  /** @brief 让 wrapper 接管一个按真实类型构造出的内部值。 */
  bool set_value(type_id dynamic_type, void* value) const;

  /** @brief 让 wrapper 接管一个按真实类型构造出的内部值，并保存销毁回调上下文。 */
  bool set_value(type_id dynamic_type, void* value,
                 const traits::wrapper_set_context& context) const;

  /** @brief 重置 wrapper 或裸指针为空。 */
  bool reset() const;

  /** @brief 确保 wrapper 持有内部值。裸指针不会自动分配内存。 */
  bool ensure_value() const;

  /** @brief 用指定内存资源确保 wrapper 持有内部值；不支持资源接管的 wrapper 会回退到普通构造。 */
  bool ensure_value(memory::memory_resource* resource) const;

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
