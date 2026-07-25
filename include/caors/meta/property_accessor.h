// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_PROPERTY_ACCESSOR_H_
#define CAORS_META_PROPERTY_ACCESSOR_H_

#include "caors/export.h"
#include "caors/meta/property.h"

#include <cstddef>

namespace caors::meta {

/**
 * @brief 已编译属性引用访问器。
 *
 * 供序列化等高层模块缓存属性热路径使用。访问器隐藏属性节点布局，只在对应稳定属性节点
 * 生命周期内有效。
 */
class CAORS_API property_accessor final {
public:
  property_accessor() noexcept = default;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

  /** @brief 从所属对象取得只读属性引用。 */
  [[nodiscard]] any_ref get_ref(const void* object) const;

  /** @brief 从所属对象取得可写属性引用。 */
  [[nodiscard]] any_ref get_ref(void* object) const;

private:
  friend property_accessor compile_property_accessor(const property& source) noexcept;

  property_accessor(const void* state, meta::type value_type, std::size_t offset, bool member,
                    bool writable, std::shared_ptr<detail::registry_layer_state> layer) noexcept
      : state_(state), value_type_(value_type), offset_(offset), member_(member),
        writable_(writable), layer_(std::move(layer)) {}

  const void* state_ = nullptr;
  meta::type value_type_{};
  std::size_t offset_ = 0;
  bool member_ = false;
  bool writable_ = false;
  std::shared_ptr<detail::registry_layer_state> layer_{};
};

/**
 * @brief 把稳定属性视图编译为窄型引用访问器。
 *
 * @return 属性无效时返回空访问器。
 */
[[nodiscard]] CAORS_API property_accessor
compile_property_accessor(const property& source) noexcept;

} // namespace caors::meta

#endif
