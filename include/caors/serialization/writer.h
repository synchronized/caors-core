// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_WRITER_H_
#define CAORS_SERIALIZATION_WRITER_H_

#include "caors/serialization/types.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caors::serialization {

/** @brief 序列化写出后端接口。 */
class CAORS_API writer {
public:
  writer() = default;
  virtual ~writer();

  writer(const writer&) = delete;
  writer& operator=(const writer&) = delete;
  writer(writer&&) = delete;
  writer& operator=(writer&&) = delete;

  [[nodiscard]] virtual format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const type_data& type) = 0;
  [[nodiscard]] virtual error begin_field(const field_data& field) = 0;
  [[nodiscard]] virtual error begin_array(std::size_t size) = 0;
  [[nodiscard]] virtual error begin_element(std::size_t index) = 0;
  [[nodiscard]] virtual error begin_dictionary(std::size_t size);
  [[nodiscard]] virtual error begin_entry(std::size_t index);
  [[nodiscard]] virtual error begin_key();
  [[nodiscard]] virtual error end_key();
  [[nodiscard]] virtual error begin_value();
  [[nodiscard]] virtual error end_value();
  [[nodiscard]] virtual error end_entry();
  [[nodiscard]] virtual error end_dictionary();
  [[nodiscard]] virtual error write_bool(bool value) = 0;
  [[nodiscard]] virtual error write_i64(std::int64_t value) = 0;
  [[nodiscard]] virtual error write_u64(std::uint64_t value) = 0;
  [[nodiscard]] virtual error write_f64(double value) = 0;
  [[nodiscard]] virtual error write_string(std::string_view value) = 0;
  [[nodiscard]] virtual error write_null();
  [[nodiscard]] virtual error end_element() = 0;
  [[nodiscard]] virtual error end_array() = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

/** @brief 简单文本序列化写出后端。 */
class CAORS_API text_writer final : public writer {
public:
  text_writer() noexcept;
  explicit text_writer(string& output);

  [[nodiscard]] format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const type_data& type) override;
  [[nodiscard]] error begin_field(const field_data& field) override;
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

  string owned_text_{};
  string* text_ = nullptr;
  vector<context> contexts_{};
  bool expecting_value_ = false;
  bool root_written_ = false;
};

/** @brief JSON 序列化写出后端。 */
class CAORS_API json_writer final : public writer {
public:
  json_writer() noexcept;
  explicit json_writer(string& output);

  [[nodiscard]] format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const type_data& type) override;
  [[nodiscard]] error begin_field(const field_data& field) override;
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

  string owned_text_{};
  string* text_ = nullptr;
  vector<context> contexts_{};
  bool expecting_value_ = false;
  bool root_written_ = false;
};

} // namespace caors::serialization

#endif
