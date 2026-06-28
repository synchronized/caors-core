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
  caors::object_registry registry;

  if (!caors::object_register_succeeded(
          caors::register_object_type<actor>(registry, actor::object_type_name))) {
    return 1;
  }

  if (!caors::object_register_succeeded(caors::register_object_type<player>(
          registry, player::object_type_name, actor::static_type_id()))) {
    return 1;
  }

  caors::object* instance = registry.create(player::static_type_id());
  if (instance == nullptr) {
    return 1;
  }

  std::cout << "object id: " << instance->id().value() << '\n';
  std::cout << "type name: " << instance->type_name() << '\n';
  std::cout << "is actor: " << registry.is_a(instance->type_id(), actor::static_type_id()) << '\n';

  registry.destroy(instance);
  return 0;
}
