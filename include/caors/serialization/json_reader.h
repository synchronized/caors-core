// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_JSON_READER_H_
#define CAORS_SERIALIZATION_JSON_READER_H_

#include "caors/serialization/json_options.h"
#include "caors/serialization/reader.h"

#include <string_view>

namespace caors::serialization {

/** @brief 从 JSON 文本范围直接读取对象字段的 reader 后端。 */
class CAORS_API json_reader final : public reader {
public:
  explicit json_reader(std::string_view text, json_options options = {},
                       memory::memory_resource* scratch_resource = nullptr);

  [[nodiscard]] format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const type_schema& type) override;
  [[nodiscard]] error begin_field(const field_schema& field) override;
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
  [[nodiscard]] bool is_null() const noexcept override;
  [[nodiscard]] bool
  is_polymorphic_envelope(const serialization::context& context) const noexcept override;
  [[nodiscard]] error read_polymorphic_type_guid(const serialization::context& context,
                                                 type_guid& value) override;
  [[nodiscard]] error begin_polymorphic_value(const serialization::context& context) override;
  [[nodiscard]] error end_polymorphic_value() override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

private:
  struct json_span {
    std::size_t begin = 0;
    std::size_t end = 0;
    bool valid = false;
  };

  struct object_member {
    string_name name{};
    json_span value{};
  };

  struct object_context {
    explicit object_context(memory::memory_resource* resource)
        : members(memory::allocator<object_member>{resource}) {}

    vector<object_member> members;
    const object_member* current = nullptr;
    std::size_t next_member = 0;
  };

  struct array_context {
    explicit array_context(memory::memory_resource* resource)
        : elements(memory::allocator<json_span>{resource}) {}

    vector<json_span> elements;
    const json_span* current = nullptr;
  };

  struct dictionary_entry {
    json_span key{};
    json_span value{};
  };

  struct dictionary_context {
    explicit dictionary_context(memory::memory_resource* resource)
        : entries(memory::allocator<dictionary_entry>{resource}) {}

    vector<dictionary_entry> entries;
    const dictionary_entry* entry = nullptr;
    json_span current{};
  };

  [[nodiscard]] const json_span* current_span() const noexcept;
  [[nodiscard]] error scan_object(json_span span, vector<object_member>& members,
                                  std::size_t depth) const;
  [[nodiscard]] error scan_array(json_span span, vector<json_span>& elements,
                                 std::size_t depth) const;
  [[nodiscard]] error scan_dictionary(json_span span, vector<dictionary_entry>& entries,
                                      std::size_t depth) const;
  [[nodiscard]] error read_string_span(json_span span, string& value) const;
  [[nodiscard]] error read_number_span(json_span span, bool& floating,
                                       std::string_view& value) const;
  [[nodiscard]] std::size_t current_container_depth() const noexcept;

  std::string_view text_{};
  json_options options_{};
  memory::memory_resource* scratch_resource_ = nullptr;
  json_span root_{};
  vector<object_context> object_stack_;
  vector<array_context> array_stack_;
  vector<dictionary_context> dictionary_stack_;
  vector<json_span> polymorphic_values_;
  vector<std::size_t> polymorphic_object_depths_;
};

} // namespace caors::serialization

#endif
