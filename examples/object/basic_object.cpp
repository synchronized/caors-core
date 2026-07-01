// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"

#include <iostream>
#include <string_view>

class actor : public caors::object_base<actor> {
public:
  static constexpr std::string_view object_type_name = "example.actor";
};

class player : public caors::object_base<player, actor> {
public:
  static constexpr std::string_view object_type_name = "example.player";
};

int main() {
  caors::object_registry& registry = caors::get_object_registry();

  if (caors::register_object_type<actor>(actor::object_type_name)) {
    return 1;
  }

  if (caors::register_object_type<player>(player::object_type_name)) {
    return 1;
  }

  const auto actor_type = registry.find_type<actor>();
  if (!actor_type.has_value()) {
    return 1;
  }

  caors::object* instance = registry.create<player>();
  if (instance == nullptr) {
    return 1;
  }

  std::cout << "object id: " << instance->id().value() << '\n';
  std::cout << "type name: " << instance->type_name() << '\n';
  std::cout << "is actor: " << registry.is_a(instance->type_id(), actor_type->id()) << '\n';

  registry.destroy(instance);
  return 0;
}
