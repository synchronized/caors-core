// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CODEC_OPS_H_
#define CAORS_SERIALIZATION_CODEC_OPS_H_

#include "caors/error.h"
#include "caors/meta/value/variant.h"
#include "caors/result.h"

namespace caors::meta {
class any;
class any_ref;
} // namespace caors::meta

namespace caors::serialization {

struct context;
class reader;
class writer;

/** @brief codec 解码前必须由调用上下文满足的外部条件。 */
enum class decode_requirement {
  none,
  borrowed_storage,
  caller_buffer,
  reference_resolver,
  object_graph,
};

/** @brief 单个类型的序列化 codec 操作表。 */
struct codec_ops {
  using write_fn = error (*)(const serialization::context& context, writer& writer,
                             const meta::any& data);
  using write_ref_fn = error (*)(const serialization::context& context, writer& writer,
                                 const meta::any_ref& data);
  using read_fn = error (*)(const serialization::context& context, reader& reader,
                            const meta::any_ref& target);
  using value_fn = result<variant> (*)(const serialization::context& context,
                                       const meta::any& data);
  using value_ref_fn = result<variant> (*)(const serialization::context& context,
                                           const meta::any_ref& data);

  write_fn write = nullptr;
  write_ref_fn write_ref = nullptr;
  read_fn read = nullptr;
  value_fn make_value = nullptr;
  value_ref_fn make_value_ref = nullptr;
  decode_requirement requirement = decode_requirement::none;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return write != nullptr || write_ref != nullptr || read != nullptr || make_value != nullptr ||
           make_value_ref != nullptr;
  }
};

} // namespace caors::serialization

#endif
