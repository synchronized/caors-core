// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_
#define CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_

#include "caors/meta/value/any_ref.h"
#include "caors/serialization/types.h"

namespace caors::serialization::detail {

/** @brief 按类型视图和序列化上下文默认构造临时运行期值。 */
[[nodiscard]] inline any default_construct_value(meta::type type,
                                                 const serialization::context& context) {
  memory::memory_resource* resource = context.effective_memory_resource();
  any decoded = any::default_construct(type, resource);
  if (!decoded.valid()) {
    decoded = any::default_construct(type.id(), resource);
  }

  return decoded;
}

} // namespace caors::serialization::detail

#endif
