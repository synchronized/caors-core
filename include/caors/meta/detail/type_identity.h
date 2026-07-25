// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_DETAIL_TYPE_IDENTITY_H_
#define CAORS_META_DETAIL_TYPE_IDENTITY_H_

#include "caors/export.h"
#include "caors/meta/type_id.h"

#include <string>
#include <string_view>

namespace caors::meta::detail {

/** @brief CAORS runtime 持有的进程内 C++ 类型身份节点。 */
struct type_identity final {
  type_id id{};
  std::string canonical_name{};
};

/**
 * @brief 按 canonical C++ 类型名称获取进程内唯一身份节点。
 *
 * 返回的节点由 CAORS runtime 持有，在 runtime 生命周期内地址稳定。名称不能为空。首次创建
 * 所需的内存分配失败或 ID 空间耗尽属于不可恢复的 runtime 初始化失败。
 */
[[nodiscard]] CAORS_API const type_identity&
get_or_create_type_identity(std::string_view canonical_name) noexcept;

} // namespace caors::meta::detail

#endif
