// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_CODEC_OPS_H_
#define CAORS_SERIALIZATION_CODEC_OPS_H_

#include "caors/error.h"
#include "caors/result.h"
#include "caors/value/variant.h"

namespace caors::value {
class any;
class any_ref;
} // namespace caors::value

namespace caors::serialization {

struct context;
class reader;
class writer;

/** @brief 单个类型的序列化 codec 操作表。 */
struct codec_ops {
  using write_fn = error (*)(const serialization::context& context, writer& writer,
                             const value::any& data);
  using read_fn = error (*)(const serialization::context& context, reader& reader,
                            const value::any_ref& target);
  using value_fn = result<variant> (*)(const serialization::context& context,
                                       const value::any& data);

  write_fn write = nullptr;
  read_fn read = nullptr;
  value_fn make_value = nullptr;

  [[nodiscard]] constexpr bool valid() const noexcept {
    return write != nullptr || read != nullptr || make_value != nullptr;
  }
};

} // namespace caors::serialization

#endif
