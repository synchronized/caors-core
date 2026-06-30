// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DOM_READER_H_
#define CAORS_SERIALIZATION_DOM_READER_H_

#include "caors/serialization/reader.h"

namespace caors {

/** @brief 从序列化 DOM 中间对象读取数据的后端。 */
class CAORS_API variant_serialization_reader final : public serialization_reader {
public:
  explicit variant_serialization_reader(const variant& value) noexcept;

  [[nodiscard]] serialization_format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const serialization_type_data& type) override;
  [[nodiscard]] error begin_field(const serialization_field_data& field) override;
  [[nodiscard]] error read_bool(bool& value) override;
  [[nodiscard]] error read_i64(std::int64_t& value) override;
  [[nodiscard]] error read_u64(std::uint64_t& value) override;
  [[nodiscard]] error read_f64(double& value) override;
  [[nodiscard]] error read_string(string& value) override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

private:
  [[nodiscard]] const variant_member* find_member(const serialization_field_data& field) const;
  [[nodiscard]] const variant* current_value() const noexcept;

  const variant* root_ = nullptr;
  const variant_member* current_member_ = nullptr;
  bool object_open_ = false;
};

} // namespace caors

#endif
