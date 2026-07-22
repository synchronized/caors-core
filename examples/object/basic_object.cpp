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
  if (caors::registration::type_<actor>().commit()) {
    return 1;
  }

  if (caors::registration::type_<player>().commit()) {
    return 1;
  }

  const auto actor_type = caors::objects::find<actor>();
  if (!actor_type) {
    return 1;
  }

  const auto player_type = caors::objects::find<player>();
  if (!player_type) {
    return 1;
  }

  caors::object* instance = caors::objects::create(player_type.id());
  if (instance == nullptr) {
    return 1;
  }

  std::cout << "object id: " << instance->id().value() << '\n';
  std::cout << "type name: " << instance->type_name() << '\n';
  std::cout << "is actor: " << caors::objects::is_a(instance->type_id(), actor_type.id()) << '\n';

  caors::objects::destroy(player_type.id(), instance);
  return 0;
}
