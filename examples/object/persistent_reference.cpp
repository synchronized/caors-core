// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/object.h"
#include "caors/reflection.h"
#include "caors/serialization.h"

#include <iostream>
#include <string_view>

class persistent_actor : public caors::object_base<persistent_actor> {
public:
  static constexpr std::string_view object_type_name = "example.persistent_actor";
};

class persistent_player : public caors::object_base<persistent_player, persistent_actor> {
public:
  static constexpr std::string_view object_type_name = "example.persistent_player";
};

struct party_slot {
  caors::object_ref<persistent_player> leader{};
};

int main() {
  if (caors::objects::class_<persistent_actor>(persistent_actor::object_type_name).commit()) {
    return 1;
  }

  if (caors::objects::class_<persistent_player>(persistent_player::object_type_name).commit()) {
    return 1;
  }

  if (caors::meta::type_<caors::object_ref<persistent_player>>(
          "caors.object_ref<example.persistent_player>")
          .result()) {
    return 1;
  }

  if (caors::reflection::class_<party_slot>("example.party_slot")
          .field("leader", &party_slot::leader)
          .commit()) {
    return 1;
  }

  if (caors::serialization::register_type<party_slot>()) {
    return 1;
  }

  const caors::object_guid player_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x60, 0x13, 0x54, 0x9d, 0x91, 0xf7, 0x40, 0x9b, 0xb9, 0x2e, 0x10,
                              0x3d, 0xf4, 0x2c, 0x0c, 0x6a})};

  caors::objects::object_index index;
  caors::objects::manager manager(index);

  persistent_player* player = manager.create_with_guid<persistent_player>(player_guid);
  if (player == nullptr) {
    return 1;
  }

  const caors::serialization::reference_resolver resolver =
      caors::serialization::make_persistent_reference_resolver(manager);
  const caors::serialization::context context{0, &resolver};

  const party_slot slot{caors::object_ref<persistent_player>{player}};
  const caors::serialization::serialization_text_result json = caors::serialize_json(slot, context);
  if (json.failed()) {
    return 1;
  }

  party_slot decoded{};
  if (caors::deserialize_json(decoded, json.value, context)) {
    return 1;
  }

  std::cout << "runtime object id: " << player->id().value() << '\n';
  std::cout << "persistent guid: " << player_guid.string() << '\n';
  std::cout << "serialized slot: " << json.value << '\n';
  std::cout << "decoded matched: " << (decoded.leader.get() == player) << '\n';

  manager.destroy(player);
  std::cout << "guid alive after destroy: " << (manager.find(player_guid) != nullptr) << '\n';

  return 0;
}
