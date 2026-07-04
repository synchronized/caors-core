// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"
#include "caors/serialization.h"

#include <iostream>
#include <string_view>

class managed_actor : public caors::object_base<managed_actor> {
public:
  static constexpr std::string_view object_type_name = "example.managed_actor";
};

class managed_player : public caors::object_base<managed_player, managed_actor> {
public:
  static constexpr std::string_view object_type_name = "example.managed_player";
};

int main() {
  if (caors::objects::class_<managed_actor>(managed_actor::object_type_name).commit()) {
    return 1;
  }

  if (caors::objects::class_<managed_player>(managed_player::object_type_name).commit()) {
    return 1;
  }

  caors::objects::manager manager;
  managed_player* player = manager.create<managed_player>();
  if (player == nullptr) {
    return 1;
  }

  const caors::objects::weak_handle<managed_player> weak =
      manager.weak<managed_player>(player->id());
  if (!weak) {
    return 1;
  }

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_reference_resolver(manager);
  const caors::object_id id = resolver.object_to_id(player, resolver.user_data);
  caors::object* resolved = resolver.id_to_object(id, resolver.user_data);

  std::cout << "managed object id: " << id.value() << '\n';
  std::cout << "weak handle alive: " << (manager.find(weak) != nullptr) << '\n';
  std::cout << "resolver matched: " << (resolved == player) << '\n';

  manager.destroy(id);
  std::cout << "weak handle alive after destroy: " << (manager.find(weak) != nullptr) << '\n';

  return 0;
}
