// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/reflection.h"
#include "caors/serialization.h"

#include <iostream>
#include <memory>
#include <utility>

namespace {

class actor {
public:
  virtual ~actor() = default;
};

class player : public actor {
public:
  explicit player(int initial_level) : level(initial_level) {}

  int level = 0;
};

struct scene {
  std::shared_ptr<actor> owner{};
};

constexpr caors::type_guid actor_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3a, 0x95, 0x13, 0x32, 0x79, 0x27, 0x49, 0x34, 0x8f, 0xd8, 0x4f, 0x8a,
                            0x94, 0x37, 0x40, 0x01})};

constexpr caors::type_guid player_guid{caors::guid::from_bytes(
    caors::guid::byte_array{0x3a, 0x95, 0x13, 0x32, 0x79, 0x27, 0x49, 0x34, 0x8f, 0xd8, 0x4f, 0x8a,
                            0x94, 0x37, 0x40, 0x02})};

bool check(caors::error status, const char* step) {
  if (!status) {
    return true;
  }

  std::cerr << "failed at " << step << ": " << static_cast<int>(status.value()) << '\n';
  return false;
}

void* construct_player(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<player>(resource, 0);
  }

  return new player(0);
}

void destroy_player(caors::memory_resource* resource, void* value) noexcept {
  auto* instance = static_cast<player*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(instance, resource);
    return;
  }

  delete instance;
}

bool register_types() {
  if (!check(caors::meta::class_<actor>("example.serialization.actor").guid(actor_guid).result(),
             "meta actor")) {
    return false;
  }

  if (!check(caors::meta::class_<player>("example.serialization.player")
                 .guid(player_guid)
                 .parent<actor>()
                 .factory(&construct_player, &destroy_player)
                 .result(),
             "meta player")) {
    return false;
  }

  if (!check(
          caors::meta::class_<std::shared_ptr<actor>>("std.shared_ptr<example.serialization.actor>")
              .result(),
          "meta shared_ptr")) {
    return false;
  }

  if (!check(caors::reflection::class_<actor>().commit(), "reflection actor")) {
    return false;
  }

  if (!check(caors::reflection::class_<player>().field("level", &player::level).commit(),
             "reflection player")) {
    return false;
  }

  if (!check(((void)caors::meta::class_<scene>("example.serialization.scene"),
              caors::reflection::class_<scene>())
                 .field("owner", &scene::owner, "std.shared_ptr<example.serialization.actor>")
                 .commit(),
             "reflection scene")) {
    return false;
  }

  return check(caors::serialization::class_<actor>().commit(), "serialization actor") &&
         check(caors::serialization::class_<player>().commit(), "serialization player") &&
         check(caors::serialization::class_<scene>().commit(), "serialization scene");
}

} // namespace

int main() {
  if (!register_types()) {
    return 1;
  }

  scene source{};
  auto owner = std::make_shared<player>(1);
  owner->level = 12;
  source.owner = std::move(owner);

  const caors::serialization::serialization_text_result json = caors::to_string(source);
  if (json.failed()) {
    std::cerr << "failed to serialize: " << static_cast<int>(json.status.value()) << '\n';
    return 1;
  }

  caors::tracking_resource tracking(&caors::get_system_memory_resource());
  caors::serialization::context context{};
  context.resolvers.memory.resource = &tracking;

  scene decoded{};
  const caors::error status = caors::deserialize(context, decoded, json.value);
  if (status) {
    std::cerr << "failed to deserialize: " << static_cast<int>(status.value()) << '\n';
    return 1;
  }

  std::cout << json.value << '\n';

  const auto* decoded_owner = dynamic_cast<const player*>(decoded.owner.get());
  if (decoded_owner == nullptr) {
    std::cerr << "decoded owner has unexpected type\n";
    return 1;
  }

  std::cout << "decoded level: " << decoded_owner->level << '\n';
  std::cout << "resource allocations: " << tracking.statistics().allocation_count << '\n';

  decoded.owner.reset();
  std::cout << "resource bytes in use: " << tracking.statistics().bytes_in_use << '\n';

  return 0;
}
