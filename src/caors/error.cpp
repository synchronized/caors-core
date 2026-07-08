// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/error.h"

namespace caors {
namespace {

class caors_error_category final : public std::error_category {
public:
  [[nodiscard]] const char* name() const noexcept override { return "caors"; }

  [[nodiscard]] std::string message(int condition) const override {
    switch (static_cast<errc>(condition)) {
    case errc::success:
      return "success";
    case errc::invalid_argument:
      return "invalid argument";
    case errc::invalid_type:
      return "invalid type";
    case errc::invalid_field:
      return "invalid field";
    case errc::missing_type:
      return "missing type";
    case errc::not_found:
      return "not found";
    case errc::already_exists:
      return "already exists";
    case errc::duplicate_id:
      return "duplicate id";
    case errc::duplicate_name:
      return "duplicate name";
    case errc::duplicate_guid:
      return "duplicate guid";
    case errc::duplicate_component:
      return "duplicate component";
    case errc::duplicate_field:
      return "duplicate field";
    case errc::duplicate_method:
      return "duplicate method";
    case errc::duplicate_reflection:
      return "duplicate reflection";
    case errc::inconsistent_type:
      return "inconsistent type";
    case errc::missing_parent:
      return "missing parent";
    case errc::frozen:
      return "frozen";
    case errc::pending:
      return "pending";
    case errc::cancelled:
      return "cancelled";
    case errc::cycle_detected:
      return "cycle detected";
    case errc::unsupported:
      return "unsupported";
    case errc::out_of_memory:
      return "out of memory";
    case errc::backend_failure:
      return "backend failure";
    }

    return "unknown caors error";
  }
};

} // namespace

const std::error_category& error_category() noexcept {
  static const caors_error_category category;
  return category;
}

std::error_code make_error_code(errc code) noexcept {
  return std::error_code{static_cast<int>(code), error_category()};
}

} // namespace caors
