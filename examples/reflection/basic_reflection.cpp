// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/reflection.h"

#include <cstddef>
#include <iostream>
#include <string_view>

struct actor {
  int id = 0;
  float health = 100.0F;
};

int main() {
  caors::reflection_registry registry;

  const caors::reflection_register_result result =
      caors::reflection::class_<actor>(registry, "example.actor")
          .field("id", &actor::id)
          .field("health", &actor::health)
          .commit();

  if (!caors::reflection_register_succeeded(result)) {
    return 1;
  }

  const auto reflected = registry.find_type("example.actor");
  if (!reflected.has_value()) {
    return 1;
  }

  std::cout << "type: " << reflected->info.name.view() << '\n';
  for (const caors::field_info& field : reflected->fields) {
    std::cout << "field: " << field.name.view() << ", type: " << field.type_name.view()
              << ", offset: " << field.offset << '\n';
  }

  return 0;
}
