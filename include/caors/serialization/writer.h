// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_WRITER_H_
#define CAORS_SERIALIZATION_WRITER_H_

#include "caors/serialization/types.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace caors {

/** @brief 序列化写出后端接口。 */
class CAORS_API serialization_writer {
public:
  serialization_writer() = default;
  virtual ~serialization_writer();

  serialization_writer(const serialization_writer&) = delete;
  serialization_writer& operator=(const serialization_writer&) = delete;
  serialization_writer(serialization_writer&&) = delete;
  serialization_writer& operator=(serialization_writer&&) = delete;

  [[nodiscard]] virtual serialization_format_features features() const noexcept = 0;
  [[nodiscard]] virtual error begin_object(const serialization_type_data& type) = 0;
  [[nodiscard]] virtual error begin_field(const serialization_field_data& field) = 0;
  [[nodiscard]] virtual error begin_array(std::size_t size) = 0;
  [[nodiscard]] virtual error begin_element(std::size_t index) = 0;
  [[nodiscard]] virtual error write_bool(bool value) = 0;
  [[nodiscard]] virtual error write_i64(std::int64_t value) = 0;
  [[nodiscard]] virtual error write_u64(std::uint64_t value) = 0;
  [[nodiscard]] virtual error write_f64(double value) = 0;
  [[nodiscard]] virtual error write_string(std::string_view value) = 0;
  [[nodiscard]] virtual error end_element() = 0;
  [[nodiscard]] virtual error end_array() = 0;
  [[nodiscard]] virtual error end_field() = 0;
  [[nodiscard]] virtual error end_object() = 0;
};

/** @brief 简单文本序列化写出后端。 */
class CAORS_API text_serialization_writer final : public serialization_writer {
public:
  text_serialization_writer() = default;

  [[nodiscard]] serialization_format_features features() const noexcept override;
  [[nodiscard]] error begin_object(const serialization_type_data& type) override;
  [[nodiscard]] error begin_field(const serialization_field_data& field) override;
  [[nodiscard]] error begin_array(std::size_t size) override;
  [[nodiscard]] error begin_element(std::size_t index) override;
  [[nodiscard]] error write_bool(bool value) override;
  [[nodiscard]] error write_i64(std::int64_t value) override;
  [[nodiscard]] error write_u64(std::uint64_t value) override;
  [[nodiscard]] error write_f64(double value) override;
  [[nodiscard]] error write_string(std::string_view value) override;
  [[nodiscard]] error end_element() override;
  [[nodiscard]] error end_array() override;
  [[nodiscard]] error end_field() override;
  [[nodiscard]] error end_object() override;

  [[nodiscard]] const string& text() const noexcept { return text_; }
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

  string text_{};
  vector<context> contexts_{};
  bool expecting_value_ = false;
  bool root_written_ = false;
};

} // namespace caors

#endif
