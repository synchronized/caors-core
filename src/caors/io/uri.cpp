// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/io/uri.h"

#include <cctype>

namespace caors::io {
namespace {

[[nodiscard]] bool is_scheme_start(char value) noexcept {
  return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] bool is_scheme_char(char value) noexcept {
  const unsigned char current = static_cast<unsigned char>(value);
  return std::isalnum(current) != 0 || value == '+' || value == '-' || value == '.';
}

[[nodiscard]] bool valid_scheme(std::string_view text) noexcept {
  if (text.empty() || !is_scheme_start(text.front())) {
    return false;
  }

  for (char value : text.substr(1)) {
    if (!is_scheme_char(value)) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] std::size_t end_of_path(std::string_view text, std::size_t begin) noexcept {
  const std::size_t query = text.find_first_of("?#", begin);
  return query == std::string_view::npos ? text.size() : query;
}

} // namespace

void uri_view::parse() noexcept {
  scheme_ = {};
  authority_ = {};
  path_ = value_;

  const std::size_t colon = value_.find(':');
  if (colon == std::string_view::npos || !valid_scheme(value_.substr(0, colon))) {
    const std::size_t path_end = end_of_path(value_, 0);
    path_ = value_.substr(0, path_end);
    return;
  }

  scheme_ = value_.substr(0, colon);
  std::size_t cursor = colon + 1;
  if (value_.substr(cursor).starts_with("//")) {
    cursor += 2;
    const std::size_t authority_end = value_.find_first_of("/?#", cursor);
    if (authority_end == std::string_view::npos) {
      authority_ = value_.substr(cursor);
      path_ = {};
      return;
    }

    authority_ = value_.substr(cursor, authority_end - cursor);
    cursor = authority_end;
    if (cursor < value_.size() && value_[cursor] == '/') {
      ++cursor;
    }
  }

  const std::size_t path_end = end_of_path(value_, cursor);
  path_ = value_.substr(cursor, path_end - cursor);
}

} // namespace caors::io
