// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_META_METHOD_REGISTRATION_H_
#define CAORS_META_METHOD_REGISTRATION_H_

#include "caors/meta/value/any.h"

#include <type_traits>
#include <utility>

namespace caors::meta::registration {

/** @brief 一条待写入的反射方法尾部默认参数。 */
struct method_default_argument_entry {
  any value{};
};

/** @brief 创建一条方法默认参数写入项。默认参数按从最右侧参数向左的顺序追加。 */
[[nodiscard]] inline method_default_argument_entry default_argument(any value) {
  return method_default_argument_entry{std::move(value)};
}

/** @brief 创建一条方法默认参数写入项。 */
template <typename Value>
[[nodiscard]] method_default_argument_entry default_argument(Value&& value) {
  using value_type = std::remove_cv_t<std::remove_reference_t<Value>>;
  return default_argument(any::from(static_cast<const value_type&>(value)));
}

} // namespace caors::meta::registration

#endif
