// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"
#include "caors/registration.h"

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
  caors::objects::registry& registry = caors::objects::get_registry();

  if (caors::registration::type_<actor>(actor::object_type_name).object().commit()) {
    return 1;
  }

  if (caors::registration::type_<player>(player::object_type_name).object().commit()) {
    return 1;
  }

  const auto actor_type = caors::objects::find<actor>();
  if (!actor_type) {
    return 1;
  }

  caors::object* instance = registry.create<player>();
  if (instance == nullptr) {
    return 1;
  }

  std::cout << "object id: " << instance->id().value() << '\n';
  std::cout << "type name: " << instance->type_name() << '\n';
  std::cout << "is actor: " << caors::objects::is_a(instance->type_id(), actor_type.id()) << '\n';

  registry.destroy(instance);
  return 0;
}
