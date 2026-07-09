// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_
#define CAORS_SERIALIZATION_DETAIL_VALUE_ACCESS_H_

#include "caors/serialization/types.h"
#include "caors/value/any_ref.h"

namespace caors::serialization::detail {

/** @brief 集中访问 `any` 携带的内部类型 descriptor。 */
[[nodiscard]] inline const meta::type_descriptor* descriptor_of(const any& value) noexcept {
  return value::detail::descriptor_of(value);
}

/** @brief 集中访问 `any_ref` 携带的内部类型 descriptor。 */
[[nodiscard]] inline const meta::type_descriptor* descriptor_of(const any_ref& value) noexcept {
  return value::detail::descriptor_of(value);
}

/** @brief 按 descriptor 和序列化上下文默认构造临时运行期值。 */
[[nodiscard]] inline any default_construct_value(type_id type,
                                                 const meta::type_descriptor* descriptor,
                                                 const serialization::context& context) {
  memory::memory_resource* resource = context.effective_memory_resource();
  any decoded = value::detail::default_construct(type, descriptor, resource);
  if (!decoded.valid()) {
    decoded = any::default_construct(type, resource);
  }

  return decoded;
}

} // namespace caors::serialization::detail

#endif
