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
  [[nodiscard]] error begin_array(std::size_t& size) override;
  [[nodiscard]] error begin_element(std::size_t index) override;
  [[nodiscard]] error begin_dictionary(std::size_t& size) override;
  [[nodiscard]] error begin_entry(std::size_t index) override;
  [[nodiscard]] error begin_key() override;
  [[nodiscard]] error end_key() override;
  [[nodiscard]] error begin_value() override;
  [[nodiscard]] error end_value() override;
  [[nodiscard]] error end_entry() override;
  [[nodiscard]] error end_dictionary() override;
  [[nodiscard]] error read_bool(bool& value) override;
  [[nodiscard]] error read_i64(std::int64_t& value) override;
  [[nodiscard]] error read_u64(std::uint64_t& value) override;
  [[nodiscard]] error read_f64(double& value) override;
  [[nodiscard]] error read_string(string& value) override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

private:
  [[nodiscard]] const variant_member* find_member(const serialization_field_data& field) const;
  [[nodiscard]] const variant* current_value() const noexcept;

  struct dictionary_context {
    const variant* source = nullptr;
    const variant_entry* entry = nullptr;
    const variant* pair = nullptr;
    const variant* current = nullptr;
  };

  const variant* root_ = nullptr;
  vector<const variant*> object_stack_{};
  vector<const variant_member*> current_members_{};
  vector<const variant*> array_stack_{};
  vector<const variant*> current_elements_{};
  vector<dictionary_context> dictionary_stack_{};
};

} // namespace caors

#endif
