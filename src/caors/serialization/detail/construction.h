// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_CONSTRUCTION_H_
#define CAORS_SERIALIZATION_DETAIL_CONSTRUCTION_H_

#include "value_access.h"

namespace caors::serialization::detail {

/** @brief 为目标引用构造一个可写的同类型临时值。 */
[[nodiscard]] inline any default_construct_for(const any_ref& target,
                                               const serialization::context& context) {
  return default_construct_value(target.get_type(), context);
}

} // namespace caors::serialization::detail

#endif
