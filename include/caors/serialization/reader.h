// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_READER_H_
#define CAORS_SERIALIZATION_READER_H_

#include "caors/serialization/types.h"

#include <cstdint>

namespace caors {

/** @brief 序列化读入后端接口。 */
class CAORS_API serialization_reader {
public:
  serialization_reader() = default;
  virtual ~serialization_reader();

  serialization_reader(const serialization_reader&) = delete;
  serialization_reader& operator=(const serialization_reader&) = delete;
  serialization_reader(serialization_reader&&) = delete;
  serialization_reader& operator=(serialization_reader&&) = delete;

  [[nodiscard]] virtual serialization_format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const serialization_type_data& type) = 0;
  [[nodiscard]] virtual error begin_field(const serialization_field_data& field) = 0;
  [[nodiscard]] virtual error read_bool(bool& value) = 0;
  [[nodiscard]] virtual error read_i64(std::int64_t& value) = 0;
  [[nodiscard]] virtual error read_u64(std::uint64_t& value) = 0;
  [[nodiscard]] virtual error read_f64(double& value) = 0;
  [[nodiscard]] virtual error read_string(string& value) = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

} // namespace caors

#endif
