// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#pragma once

#include "caors/object.h"
#include "caors/registration.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace caors::tests::object {

class runtime_actor : public caors::object_base<runtime_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.runtime_actor";
};

class runtime_player : public caors::object_base<runtime_player, runtime_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.runtime_player";
};

class registry_actor : public caors::object_base<registry_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.registry_actor";
};

class persistent_actor : public caors::object_base<persistent_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.persistent_actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0x92, 0x8d, 0x35, 0x70, 0x54, 0x68, 0x46, 0x5c, 0x9d, 0xc4, 0x30,
                              0x7f, 0xb7, 0x45, 0xf2, 0x2c})};
};

class error_actor : public caors::object_base<error_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.error_actor";
};

class error_player : public caors::object_base<error_player, error_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.error_player";
};

class guid_owner_actor : public caors::object_base<guid_owner_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.guid_owner_actor";
  static constexpr caors::type_guid object_type_guid{caors::guid::from_bytes(
      caors::guid::byte_array{0xf8, 0x23, 0x42, 0x4e, 0xc4, 0x3b, 0x4a, 0xe2, 0x9a, 0x8e, 0x55,
                              0xc7, 0xe4, 0xf8, 0x3a, 0x11})};
};

class duplicate_guid_player : public caors::object_base<duplicate_guid_player> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.duplicate_guid_player";
  static constexpr caors::type_guid object_type_guid = guid_owner_actor::object_type_guid;
};

class relation_actor : public caors::object_base<relation_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.relation_actor";
};

class relation_player : public caors::object_base<relation_player, relation_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.relation_player";
};

class factory_actor : public caors::object_base<factory_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.factory_actor";
};

class registered_non_default_object : public caors::object_base<registered_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.registered_non_default";

  explicit registered_non_default_object(int value) : value(value) {}

  int value = 0;
};

class direct_non_default_object : public caors::object_base<direct_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.direct_non_default";

  explicit direct_non_default_object(int value) : value(value) {}

  int value = 0;
};

class managed_actor : public caors::object_base<managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_actor";

  int value = 0;
};

class managed_player : public caors::object_base<managed_player, managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_player";
};

class managed_other_actor : public caors::object_base<managed_other_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_other_actor";
};

class managed_non_default_object : public caors::object_base<managed_non_default_object> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.managed_non_default";

  explicit managed_non_default_object(int value) : value(value) {}

  int value = 0;
};

class unregistered_managed_actor : public caors::object_base<unregistered_managed_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.unregistered_managed";
};

class destruction_observer : public caors::object_base<destruction_observer> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.destruction_observer";

  ~destruction_observer() override {
    invisible_during_destruction = manager != nullptr && manager->find(id()) == nullptr;
  }

  inline static caors::objects::manager* manager = nullptr;
  inline static bool invisible_during_destruction = false;
};

class delayed_destroy_actor : public caors::object_base<delayed_destroy_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.delayed_destroy_actor";

  ~delayed_destroy_actor() override { ++destroyed_count; }

  int value = 0;
  inline static int destroyed_count = 0;
};

class counted_actor : public caors::object_base<counted_actor>, public caors::objects::ref_counted {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.counted_actor";

  explicit counted_actor(int value = 0) : value(value) {}

  ~counted_actor() override { ++destroyed_count; }

  int value = 0;
  inline static int destroyed_count = 0;
};

class counted_player : public caors::object_base<counted_player, counted_actor> {
public:
  static constexpr std::string_view object_type_name = "caors.test.object.counted_player";

  ~counted_player() override { ++destroyed_count; }

  inline static int destroyed_count = 0;
};

constexpr caors::object_guid test_object_guid_a{caors::guid::from_bytes(
    caors::guid::byte_array{0x18, 0x50, 0xf2, 0x4f, 0x4f, 0xf8, 0x47, 0x66, 0x9f, 0x38, 0x2b, 0x79,
                            0xc9, 0xd7, 0x3a, 0x01})};

constexpr caors::object_guid test_object_guid_b{caors::guid::from_bytes(
    caors::guid::byte_array{0x18, 0x50, 0xf2, 0x4f, 0x4f, 0xf8, 0x47, 0x66, 0x9f, 0x38, 0x2b, 0x79,
                            0xc9, 0xd7, 0x3a, 0x02})};

[[nodiscard]] constexpr caors::error object_error(caors::errc code) noexcept {
  return {caors::error_domain::object, code};
}

template <typename T>
[[nodiscard]] caors::error
register_object_test_type(caors::object_type_id parent_id = caors::object_type_id::invalid()) {
  auto builder = caors::registration::type_<T>();
  auto object = builder.object();
  if (parent_id) {
    object.parent(parent_id);
  }

  return caors::rethrow_as(object.commit(), caors::error_domain::object);
}

template <typename T>
[[nodiscard]] caors::error
register_object_test_type(std::string_view name,
                          caors::object_type_id parent_id = caors::object_type_id::invalid()) {
  auto builder = caors::registration::type_<T>(name);
  auto object = builder.object();
  if (parent_id) {
    object.parent(parent_id);
  }

  return caors::rethrow_as(object.commit(), caors::error_domain::object);
}

} // namespace caors::tests::object
