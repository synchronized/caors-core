// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_CODEC_OPS_H_
#define CAORS_SERIALIZATION_DETAIL_CODEC_OPS_H_

#include "caors/error.h"
#include "caors/meta/type_id.h"
#include "caors/result.h"
#include "caors/value/variant.h"

namespace caors::reflection {
class field;
} // namespace caors::reflection

namespace caors::value {
class any;
class any_ref;
} // namespace caors::value

namespace caors::serialization {
struct context;
class reader;
class writer;
} // namespace caors::serialization

namespace caors::serialization::detail {

using codec_write_fn = error (*)(const serialization::context& context, writer& writer,
                                 const value::any& data);
using codec_read_fn = error (*)(const serialization::context& context, reader& reader,
                                const reflection::field& field, void* object);
using codec_read_value_fn = error (*)(const serialization::context& context, reader& reader,
                                      const value::any_ref& target);
using codec_value_fn = result<variant> (*)(const serialization::context& context,
                                           const value::any& data);

/** @brief 单个类型的序列化 codec 操作表。 */
struct codec_ops {
  type_id type{};
  codec_write_fn write = nullptr;
  codec_read_fn read = nullptr;
  codec_read_value_fn read_value = nullptr;
  codec_value_fn make_value = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return type &&
           (write != nullptr || read != nullptr || read_value != nullptr || make_value != nullptr);
  }
};

} // namespace caors::serialization::detail

#endif
