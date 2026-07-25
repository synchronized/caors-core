// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/semantic_version.h"

#include <charconv>
#include <limits>

namespace caors::plugins {
namespace {

[[nodiscard]] error invalid_version() noexcept {
  return {error_domain::plugins, errc::invalid_argument};
}

[[nodiscard]] bool parse_component(std::string_view text, std::uint32_t& value) {
  if (text.empty()) {
    return false;
  }
  const char* const end = text.data() + text.size();
  const auto parsed = std::from_chars(text.data(), end, value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

} // namespace

result<semantic_version> semantic_version::parse(std::string_view text) {
  semantic_version value{};
  const std::size_t first = text.find('.');
  const std::size_t second = first == std::string_view::npos ? first : text.find('.', first + 1);
  if (first == std::string_view::npos || second == std::string_view::npos ||
      text.find('.', second + 1) != std::string_view::npos ||
      !parse_component(text.substr(0, first), value.major) ||
      !parse_component(text.substr(first + 1, second - first - 1), value.minor) ||
      !parse_component(text.substr(second + 1), value.patch)) {
    return fail<semantic_version>(invalid_version());
  }
  return ok(value);
}

std::string semantic_version::string() const {
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

result<version_range> version_range::parse(std::string_view text) {
  version_range range{};
  if (text.empty() || text == "*") {
    return ok(range);
  }

  std::size_t position = 0;
  bool saw_constraint = false;
  while (position < text.size()) {
    while (position < text.size() && text[position] == ' ') {
      ++position;
    }
    if (position == text.size()) {
      break;
    }
    const std::size_t end = text.find(' ', position);
    const std::string_view token = text.substr(position, end - position);
    position = end == std::string_view::npos ? text.size() : end + 1;

    std::string_view version_text = token;
    enum class comparison { exact, minimum, maximum } operation = comparison::exact;
    bool inclusive = true;
    if (token.starts_with(">=") || token.starts_with("<=")) {
      operation = token.front() == '>' ? comparison::minimum : comparison::maximum;
      version_text.remove_prefix(2);
    } else if (token.starts_with('>') || token.starts_with('<')) {
      operation = token.front() == '>' ? comparison::minimum : comparison::maximum;
      inclusive = false;
      version_text.remove_prefix(1);
    } else if (token.starts_with('=')) {
      version_text.remove_prefix(1);
    }

    auto parsed = semantic_version::parse(version_text);
    if (!parsed) {
      return fail<version_range>(parsed.error());
    }
    saw_constraint = true;
    if (operation == comparison::exact) {
      if (range.minimum || range.maximum) {
        return fail<version_range>(invalid_version());
      }
      range.minimum = *parsed;
      range.maximum = *parsed;
    } else if (operation == comparison::minimum) {
      if (range.minimum) {
        return fail<version_range>(invalid_version());
      }
      range.minimum = *parsed;
      range.minimum_inclusive = inclusive;
    } else {
      if (range.maximum) {
        return fail<version_range>(invalid_version());
      }
      range.maximum = *parsed;
      range.maximum_inclusive = inclusive;
    }
  }

  if (!saw_constraint || (range.minimum && range.maximum &&
                          (*range.minimum > *range.maximum ||
                           (*range.minimum == *range.maximum &&
                            (!range.minimum_inclusive || !range.maximum_inclusive))))) {
    return fail<version_range>(invalid_version());
  }
  return ok(range);
}

bool version_range::contains(semantic_version value) const noexcept {
  if (minimum && (value < *minimum || (value == *minimum && !minimum_inclusive))) {
    return false;
  }
  if (maximum && (value > *maximum || (value == *maximum && !maximum_inclusive))) {
    return false;
  }
  return true;
}

} // namespace caors::plugins
