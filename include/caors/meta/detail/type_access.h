// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_ACCESS_H_
#define CAORS_META_DETAIL_TYPE_ACCESS_H_

#include "caors/meta/detail/type_record.h"
#include "caors/meta/type.h"

namespace caors::meta::detail {

/**
 * @brief `meta::type` 的内部访问口。
 *
 * 普通代码不应使用本类型。它只用于 meta 内部实现跨翻译单元访问 `type` 私有状态，
 * 避免把多个 detail 自由函数都声明为 `type` 的 friend。
 */
class type_access final {
public:
  [[nodiscard]] static constexpr type make(const registry* registry,
                                           const type_record* data) noexcept {
    return type{registry, data};
  }

  [[nodiscard]] static constexpr const registry* registry_of(const type& value) noexcept {
    return value.registry_;
  }

  [[nodiscard]] static constexpr const type_record* data_of(const type& value) noexcept {
    return value.data_;
  }

  [[nodiscard]] static const type_record* registered_data_of(const type& value) noexcept {
    const type_record* data = data_of(value);
    return data != nullptr && data->registered() ? data : nullptr;
  }
};

} // namespace caors::meta::detail

#endif
