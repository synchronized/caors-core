// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_REGISTRATION_H_
#define CAORS_META_REGISTRATION_H_

#include "caors/error.h"
#include "caors/export.h"

namespace caors::meta {

/** @brief 延迟到 `meta::run_registrations()` 前执行的元数据注册函数。 */
using registration_fn = error (*)();

namespace detail {

/** @brief 注册一个延迟执行的元数据注册函数。内部入口，优先使用 `CAORS_META_REGISTRATION`。 */
[[nodiscard]] CAORS_API error register_registration(registration_fn fn);

} // namespace detail

/** @brief 执行所有已收集的元数据注册函数；最多执行一次。 */
[[nodiscard]] CAORS_API error run_registrations();

/** @brief 冻结全局元数据注册表；冻结后只允许只读查询。 */
[[nodiscard]] CAORS_API error freeze_registry();

/** @brief 查询全局元数据注册表是否已经冻结。 */
[[nodiscard]] CAORS_API bool is_registry_frozen() noexcept;

} // namespace caors::meta

/**
 * @brief 定义一个延迟到 `meta::run_registrations()` 前执行的元数据注册函数。
 *
 * `name` 必须是当前翻译单元内唯一的普通标识符。函数体需要返回 `caors::error`。
 */
#define CAORS_META_REGISTRATION(name)                                                              \
  static ::caors::error name();                                                                    \
  namespace {                                                                                      \
  struct name##_caors_meta_registration_auto_register {                                            \
    name##_caors_meta_registration_auto_register() {                                               \
      (void)::caors::meta::detail::register_registration(&name);                                   \
    }                                                                                              \
  };                                                                                               \
  static const name##_caors_meta_registration_auto_register                                        \
      name##_caors_meta_registration_auto_register_instance{};                                     \
  }                                                                                                \
  static ::caors::error name()

#endif
