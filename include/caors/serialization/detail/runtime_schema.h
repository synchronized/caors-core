// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_RUNTIME_SCHEMA_H_
#define CAORS_SERIALIZATION_DETAIL_RUNTIME_SCHEMA_H_

#include "caors/error.h"
#include "caors/meta/type.h"
#include "caors/serialization/type_schema.h"

namespace caors::serialization::detail {

/** @brief 在类型注册提交后编译内部字段调度缓存。 */
[[nodiscard]] CAORS_API error compile_type_schema(const meta::type& owner_type,
                                                  serialization::type_schema& schema);

} // namespace caors::serialization::detail

#endif
