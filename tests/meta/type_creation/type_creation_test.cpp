// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/memory.h"
#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

struct meta_default_create_type {
  int value = 11;
};

struct meta_factory_create_type {
  meta_factory_create_type() = default;
  explicit meta_factory_create_type(int input) : value(input) {}

  int value = 1;
};

struct meta_resource_factory_create_type {
  explicit meta_resource_factory_create_type(int input) : value(input) {}

  int value = 0;
};

struct meta_constructor_create_type {
  explicit meta_constructor_create_type(int input) : value(input) {}

  int value = 0;
};

struct meta_rvalue_constructor_create_type {
  explicit meta_rvalue_constructor_create_type(int&& input) : value(input) { input = -1; }

  int value = 0;
};

struct meta_factory_constructor_priority_type {
  meta_factory_constructor_priority_type() = default;

  int value = 3;
};

struct meta_missing_create_type {
  explicit meta_missing_create_type(int input) : value(input) {}

  int value = 0;
};

void* construct_meta_factory_create_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<meta_factory_create_type>(resource, 42);
  }

  return new meta_factory_create_type(42);
}

void destroy_meta_factory_create_type(caors::memory_resource* resource, void* value) noexcept {
  auto* typed = static_cast<meta_factory_create_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

void* construct_meta_resource_factory_create_type(caors::memory_resource* resource) {
  if (resource != nullptr) {
    return caors::memory::make<meta_resource_factory_create_type>(resource, 43);
  }

  return new meta_resource_factory_create_type(43);
}

void destroy_meta_resource_factory_create_type(caors::memory_resource* resource,
                                               void* value) noexcept {
  auto* typed = static_cast<meta_resource_factory_create_type*>(value);
  if (resource != nullptr) {
    caors::memory::destroy(typed, resource);
    return;
  }

  delete typed;
}

void* construct_meta_factory_constructor_priority_type(caors::memory_resource* resource) {
  auto* value = caors::memory::make<meta_factory_constructor_priority_type>(resource);
  if (value != nullptr) {
    value->value = 44;
  }
  return value;
}

void destroy_meta_factory_constructor_priority_type(caors::memory_resource* resource,
                                                    void* value) noexcept {
  caors::memory::destroy(static_cast<meta_factory_constructor_priority_type*>(value), resource);
}

} // namespace

TEST_CASE("meta type creation - uses registered factory before default construction",
          "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_factory_create_type>("caors.test.meta.factory_create")
          .factory(&construct_meta_factory_create_type, &destroy_meta_factory_create_type)
          .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_factory_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.value_constructible());

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<meta_factory_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 42);

  caors::any default_value = type.create_value();
  REQUIRE(default_value.valid());
  auto* default_created = default_value.try_get_mutable<meta_factory_create_type>();
  REQUIRE(default_created != nullptr);
  CHECK(default_created->value == 1);
}

TEST_CASE("meta type creation - falls back to descriptor default construction",
          "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_default_create_type>("caors.test.meta.default_create").commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_default_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.value_constructible());

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<meta_default_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 11);
}

TEST_CASE("meta type creation - reports non constructible types", "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_missing_create_type>("caors.test.meta.missing_create").commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_missing_create_type>();
  REQUIRE(type.valid());
  CHECK_FALSE(type.constructible());
  CHECK_FALSE(type.value_constructible());
  CHECK_FALSE(type.create());
  CHECK_FALSE(type.create_value());
}

TEST_CASE("meta type creation - keeps factory resource ownership", "[meta][type_creation]") {
  const caors::error status = caors::meta::class_<meta_resource_factory_create_type>(
                                  "caors.test.meta.resource_factory_create")
                                  .factory(&construct_meta_resource_factory_create_type,
                                           &destroy_meta_resource_factory_create_type)
                                  .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_resource_factory_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK_FALSE(type.value_constructible());

  caors::tracking_resource tracking{&caors::get_system_memory_resource()};

  caors::any value = type.create(&tracking);
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<meta_resource_factory_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 43);
  CHECK(tracking.statistics().allocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use >= sizeof(meta_resource_factory_create_type));

  value.reset();
  CHECK(tracking.statistics().deallocation_count == 1);
  CHECK(tracking.statistics().bytes_in_use == 0);
}

TEST_CASE("meta type creation - uses registered constructor arguments", "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_constructor_create_type>("caors.test.meta.constructor_create")
          .constructor<int>()
          .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_constructor_create_type>();
  REQUIRE(type.valid());
  CHECK_FALSE(type.constructible());
  CHECK(type.constructible(1));
  CHECK_FALSE(type.value_constructible());
  CHECK_FALSE(type.create());

  int input = 37;
  std::array<caors::argument, 1> arguments{caors::arg(input)};
  caors::any value =
      type.create(std::span<const caors::argument>{arguments.data(), arguments.size()});
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<meta_constructor_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 37);

  float wrong = 3.0F;
  std::array<caors::argument, 1> wrong_arguments{caors::arg(wrong)};
  CHECK_FALSE(type.create(
      std::span<const caors::argument>{wrong_arguments.data(), wrong_arguments.size()}));
}

TEST_CASE("meta type creation - supports rvalue constructor arguments", "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_rvalue_constructor_create_type>(
          "caors.test.meta.rvalue_constructor_create")
          .constructor<int&&>()
          .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_rvalue_constructor_create_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible(1));

  int input = 17;
  std::array<caors::argument, 1> lvalue_arguments{caors::arg(input)};
  CHECK_FALSE(type.create(
      std::span<const caors::argument>{lvalue_arguments.data(), lvalue_arguments.size()}));
  CHECK(input == 17);

  std::array<caors::argument, 1> rvalue_arguments{caors::move_arg(21)};
  caors::any value = type.create(
      std::span<const caors::argument>{rvalue_arguments.data(), rvalue_arguments.size()});
  REQUIRE(value.valid());
  const auto* created = value.try_get<meta_rvalue_constructor_create_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 21);
}

TEST_CASE("meta type creation - keeps factory before zero argument constructor",
          "[meta][type_creation]") {
  const caors::error status =
      caors::meta::class_<meta_factory_constructor_priority_type>(
          "caors.test.meta.factory_constructor_priority")
          .factory(&construct_meta_factory_constructor_priority_type,
                   &destroy_meta_factory_constructor_priority_type)
          .constructor<>()
          .commit();
  CHECK_FALSE(status);

  caors::meta::type type = caors::meta::find<meta_factory_constructor_priority_type>();
  REQUIRE(type.valid());
  CHECK(type.constructible());
  CHECK(type.constructible(0));

  caors::any value = type.create();
  REQUIRE(value.valid());
  auto* created = value.try_get_mutable<meta_factory_constructor_priority_type>();
  REQUIRE(created != nullptr);
  CHECK(created->value == 44);
}
