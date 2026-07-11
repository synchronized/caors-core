// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_READER_H_
#define CAORS_SERIALIZATION_READER_H_

#include "caors/serialization/types.h"

#include <cstdint>

namespace caors::serialization {

/** @brief 序列化读入后端接口。 */
class CAORS_API reader {
public:
  reader() = default;
  virtual ~reader();

  reader(const reader&) = delete;
  reader& operator=(const reader&) = delete;
  reader(reader&&) = delete;
  reader& operator=(reader&&) = delete;

  [[nodiscard]] virtual format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const type_schema& type) = 0;
  [[nodiscard]] virtual error begin_field(const field_schema& field) = 0;
  [[nodiscard]] virtual error begin_array(std::size_t& size) = 0;
  [[nodiscard]] virtual error begin_element(std::size_t index) = 0;
  [[nodiscard]] virtual error begin_dictionary(std::size_t& size);
  [[nodiscard]] virtual error begin_entry(std::size_t index);
  [[nodiscard]] virtual error begin_key();
  [[nodiscard]] virtual error end_key();
  [[nodiscard]] virtual error begin_value();
  [[nodiscard]] virtual error end_value();
  [[nodiscard]] virtual error end_entry();
  [[nodiscard]] virtual error end_dictionary();
  [[nodiscard]] virtual error read_bool(bool& value) = 0;
  [[nodiscard]] virtual error read_i64(std::int64_t& value) = 0;
  [[nodiscard]] virtual error read_u64(std::uint64_t& value) = 0;
  [[nodiscard]] virtual error read_f64(double& value) = 0;
  [[nodiscard]] virtual error read_string(string& value) = 0;
  [[nodiscard]] virtual bool is_null() const noexcept;
  [[nodiscard]] virtual const variant* current_variant() const noexcept;
  [[nodiscard]] virtual error end_element() = 0;
  [[nodiscard]] virtual error end_array() = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

} // namespace caors::serialization

#endif
