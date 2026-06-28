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
  const caors::reflection_register_result result = caors::reflection::class_<actor>("example.actor")
                                                       .field("id", &actor::id)
                                                       .field("health", &actor::health)
                                                       .commit();

  if (!caors::reflection_register_succeeded(result)) {
    return 1;
  }

  const auto reflected = caors::find_reflection("example.actor");
  if (!reflected.has_value()) {
    return 1;
  }

  std::cout << "type: " << reflected->name().view() << '\n';
  for (std::size_t index = 0; index < reflected->field_count(); ++index) {
    const auto field = reflected->field_at(index);
    if (!field.has_value()) {
      continue;
    }

    std::cout << "field: " << field->name().view() << ", type: " << field->type_name().view()
              << ", offset: " << field->offset() << '\n';
  }

  actor instance{};
  const auto health = reflected->find_field("health");
  if (health.has_value() && health->try_set<float>(&instance, 75.0F)) {
    const auto value = health->try_get<float>(&instance);
    if (value.has_value()) {
      std::cout << "health: " << *value << '\n';
    }
  }

  return 0;
}
