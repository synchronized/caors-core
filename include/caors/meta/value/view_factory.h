// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_VALUE_VIEW_FACTORY_H_
#define CAORS_META_VALUE_VIEW_FACTORY_H_

#include "caors/meta/value/associative_view.h"
#include "caors/meta/value/indirect_view.h"
#include "caors/meta/value/pair_view.h"
#include "caors/meta/value/sequence_view.h"
#include "caors/meta/value/set_view.h"
#include "caors/meta/value/tuple_view.h"
#include "caors/meta/value/variant_view.h"

namespace caors::meta {

/** @brief 从运行期引用创建指针或 wrapper 内部值视图。 */
[[nodiscard]] indirect_view make_indirect_view(any_ref source) noexcept;

/** @brief 从运行期引用创建顺序容器视图。 */
[[nodiscard]] sequence_view make_sequence_view(any_ref source) noexcept;

/** @brief 从运行期引用创建集合容器视图。 */
[[nodiscard]] set_view make_set_view(any_ref source) noexcept;

/** @brief 从运行期引用创建关联容器视图。 */
[[nodiscard]] associative_view make_associative_view(any_ref source) noexcept;

/** @brief 从运行期引用创建 pair-like 视图。 */
[[nodiscard]] pair_view make_pair_view(any_ref source) noexcept;

/** @brief 从运行期引用创建 tuple-like 视图。 */
[[nodiscard]] tuple_view make_tuple_view(any_ref source) noexcept;

/** @brief 从运行期引用创建 variant-like 视图。 */
[[nodiscard]] variant_view make_variant_view(any_ref source) noexcept;

} // namespace caors::meta

#endif
