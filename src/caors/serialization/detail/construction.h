// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_CONSTRUCTION_H_
#define CAORS_SERIALIZATION_DETAIL_CONSTRUCTION_H_

#include "caors/serialization/types.h"
#include "caors/value/any_ref.h"

namespace caors::serialization::detail {

/** @brief 按 descriptor 和序列化上下文默认构造临时运行期值。 */
[[nodiscard]] inline any default_construct_value(type_id type,
                                                 const meta::detail::type_descriptor* descriptor,
                                                 const serialization::context& context) {
  any decoded = value::detail::default_construct(type, descriptor, context.memory_resource);
  if (!decoded.valid()) {
    decoded = any::default_construct(type, context.memory_resource);
  }

  return decoded;
}

/** @brief 为目标引用构造一个可写的同类型临时值。 */
[[nodiscard]] inline any default_construct_for(const any_ref& target,
                                               const serialization::context& context) {
  return default_construct_value(target.type(), value::detail::descriptor_of(target), context);
}

} // namespace caors::serialization::detail

#endif
