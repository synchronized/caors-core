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
  if (caors::objects::class_<managed_actor>().commit()) {
    return 1;
  }

  if (caors::objects::class_<managed_player>().commit()) {
    return 1;
  }

  caors::objects::manager manager;
  managed_player* player = manager.create<managed_player>();
  if (player == nullptr) {
    return 1;
  }

  const caors::objects::typed_handle<managed_player> typed =
      manager.handle_of<managed_player>(player->id());
  if (!typed) {
    return 1;
  }

  const caors::object_ref_resolver resolver = caors::make_object_ref_resolver(manager);
  caors::serialization::external_ref ref{};
  if (!resolver.encode_ref(player, player->type_id(), ref)) {
    return 1;
  }

  const caors::object_id id{ref.integer_id};
  auto* resolved = static_cast<caors::object*>(resolver(ref));

  std::cout << "managed object id: " << id.value() << '\n';
  std::cout << "typed handle alive: " << (manager.find(typed) != nullptr) << '\n';
  std::cout << "resolver matched: " << (resolved == player) << '\n';

  manager.destroy(id);
  std::cout << "typed handle alive after destroy: " << (manager.find(typed) != nullptr) << '\n';

  return 0;
}
