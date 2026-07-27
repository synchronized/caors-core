// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_SCHEMA_REGISTRATION_H_
#define CAORS_SERIALIZATION_SCHEMA_REGISTRATION_H_

#include "caors/error.h"
#include "caors/meta/type.h"
#include "caors/serialization/type_schema.h"

namespace caors::serialization {

/**
 * @brief 为已注册类型准备完整序列化 schema。
 *
 * 未显式配置字段时根据类型的本地属性生成默认字段；随后验证字段引用并编译内部运行时分派缓存。
 *
 * @param owner_type 已提交到 registry 的类型。
 * @param schema 待准备的 schema；成功后可作为类型 capability data 提交。
 * @return 成功返回空错误；类型、字段或属性无效时返回对应错误。
 */
[[nodiscard]] CAORS_API error prepare_type_schema(const meta::type& owner_type,
                                                  type_schema& schema);

} // namespace caors::serialization

#endif
