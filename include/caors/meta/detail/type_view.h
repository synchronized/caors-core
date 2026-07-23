// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_VIEW_H_
#define CAORS_META_DETAIL_TYPE_VIEW_H_

#include "caors/meta/type.h"

namespace caors::meta::detail {

/** @brief 创建未注册或已注册 C++ 类型的内部只读视图。 */
[[nodiscard]] CAORS_API type make_type_view(type_id id, const type_data* fallback) noexcept;

/** @brief 使用指定注册表创建 C++ 类型的内部只读视图。 */
[[nodiscard]] CAORS_API type make_type_view(registry& registry, type_id id,
                                            const type_data* fallback) noexcept;

} // namespace caors::meta::detail

#endif
