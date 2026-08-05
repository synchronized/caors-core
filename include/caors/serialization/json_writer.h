// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_JSON_WRITER_H_
#define CAORS_SERIALIZATION_JSON_WRITER_H_

#include "caors/io/stream.h"
#include "caors/serialization/json_options.h"
#include "caors/serialization/writer.h"

namespace caors::serialization {

/** @brief JSON 序列化写出后端。 */
class CAORS_API json_writer final : public writer {
public:
  json_writer() noexcept;
  explicit json_writer(json_options options) noexcept;
  json_writer(json_options options, memory::memory_resource* scratch_resource) noexcept;
  explicit json_writer(string& output);
  json_writer(string& output, json_options options);
  json_writer(string& output, json_options options, memory::memory_resource* scratch_resource);
  explicit json_writer(io::output_stream& output) noexcept;
  json_writer(io::output_stream& output, json_options options) noexcept;
  json_writer(io::output_stream& output, json_options options,
              memory::memory_resource* scratch_resource) noexcept;

  [[nodiscard]] format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const type_schema& type) override;
  [[nodiscard]] error begin_field(const field_schema& field) override;
  [[nodiscard]] error begin_array(std::size_t size) override;
  [[nodiscard]] error begin_element(std::size_t index) override;
  [[nodiscard]] error begin_dictionary(std::size_t size) override;
  [[nodiscard]] error begin_entry(std::size_t index) override;
  [[nodiscard]] error begin_key() override;
  [[nodiscard]] error end_key() override;
  [[nodiscard]] error begin_value() override;
  [[nodiscard]] error end_value() override;
  [[nodiscard]] error end_entry() override;
  [[nodiscard]] error end_dictionary() override;
  [[nodiscard]] error write_bool(bool value) override;
  [[nodiscard]] error write_i64(std::int64_t value) override;
  [[nodiscard]] error write_u64(std::uint64_t value) override;
  [[nodiscard]] error write_f64(double value) override;
  [[nodiscard]] error write_string(std::string_view value) override;
  [[nodiscard]] error write_null() override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

  [[nodiscard]] const string& text() const noexcept { return *text_; }
  void reset();

private:
  enum class context_kind {
    object,
    array,
  };

  struct context {
    context_kind kind = context_kind::object;
    bool first = true;
  };

  [[nodiscard]] error prepare_value();
  void mark_value_written() noexcept;
  [[nodiscard]] string& output() noexcept { return *text_; }
  [[nodiscard]] error write_char(char value);
  [[nodiscard]] error write_text(std::string_view value);
  [[nodiscard]] error write_json_string(std::string_view value);
  [[nodiscard]] error write_newline_and_indent(std::size_t depth);

  json_options options_{};
  string owned_text_{};
  string* text_ = nullptr;
  io::output_stream* stream_ = nullptr;
  vector<context> contexts_;
  bool expecting_value_ = false;
  bool root_written_ = false;
};

} // namespace caors::serialization

#endif
