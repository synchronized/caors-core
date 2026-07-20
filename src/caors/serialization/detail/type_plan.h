// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_SERIALIZATION_DETAIL_TYPE_PLAN_H_
#define CAORS_SERIALIZATION_DETAIL_TYPE_PLAN_H_

#include "caors/meta/property.h"
#include "caors/meta/type.h"
#include "caors/result.h"
#include "caors/serialization/type_schema.h"

namespace caors::serialization::detail {

struct planned_field {
  serialization::field_schema schema{};
  meta::property property{};
};

struct type_plan {
  const serialization::type_schema* source_schema = nullptr;
  serialization::type_schema object_schema{};
  vector<planned_field> fields{};
  std::size_t property_count = 0;
  bool has_explicit_fields = false;

  [[nodiscard]] bool valid() const noexcept { return source_schema != nullptr; }
};

[[nodiscard]] result<const type_plan*> find_type_plan(const meta::type& type);

} // namespace caors::serialization::detail

#endif
