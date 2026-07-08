// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_CODEC_DESCRIPTOR_H_
#define CAORS_SERIALIZATION_DETAIL_CODEC_DESCRIPTOR_H_

#include "caors/error.h"
#include "caors/meta/type_id.h"
#include "caors/result.h"
#include "caors/value/variant.h"

namespace caors::meta::detail {
struct type_descriptor;
} // namespace caors::meta::detail

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

using codec_write_fn = error (*)(writer& writer, const value::any& data);
using codec_context_write_fn = error (*)(const serialization::context& context, writer& writer,
                                         const value::any& data);
using codec_read_fn = error (*)(reader& reader, const reflection::field& field, void* object);
using codec_context_read_fn = error (*)(const serialization::context& context, reader& reader,
                                        const reflection::field& field, void* object);
using codec_read_value_fn = error (*)(reader& reader, const value::any_ref& target);
using codec_context_read_value_fn = error (*)(const serialization::context& context, reader& reader,
                                              const value::any_ref& target);
using codec_value_fn = result<variant> (*)(const value::any& data);
using codec_context_value_fn = result<variant> (*)(const serialization::context& context,
                                                   const value::any& data);

/** @brief 单个类型的序列化 codec 能力描述。 */
struct codec_descriptor {
  type_id type{};
  const meta::detail::type_descriptor* value_type = nullptr;
  codec_write_fn write = nullptr;
  codec_context_write_fn write_context = nullptr;
  codec_read_fn read = nullptr;
  codec_context_read_fn read_context = nullptr;
  codec_read_value_fn read_value = nullptr;
  codec_context_read_value_fn read_value_context = nullptr;
  codec_value_fn make_value = nullptr;
  codec_context_value_fn make_value_context = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return type && value_type != nullptr &&
           (write != nullptr || write_context != nullptr || read != nullptr ||
            read_context != nullptr || read_value != nullptr || read_value_context != nullptr ||
            make_value != nullptr || make_value_context != nullptr);
  }
};

using codec_descriptor_provider_fn = const codec_descriptor* (*)();

/** @brief descriptor 级静态 codec 能力，延迟取得实际 codec 函数表。 */
struct codec_capability {
  codec_descriptor_provider_fn descriptor = nullptr;

  [[nodiscard]] const codec_descriptor* get() const {
    return descriptor != nullptr ? descriptor() : nullptr;
  }

  [[nodiscard]] bool valid() const { return get() != nullptr; }
};

} // namespace caors::serialization::detail

#endif
