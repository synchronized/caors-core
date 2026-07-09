// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <string_view>

namespace {

struct runtime_capability {
  int value = 0;
};

struct runtime_capability_type {};

struct static_capability_data {
  int value = 0;
};

struct static_capability_type {};

struct named_capability {};

struct first_extension_capability {
  int value = 0;
};

struct second_extension_capability {
  int value = 0;
};

struct extension_capability_type {};

} // namespace

namespace caors::meta {

template <> struct capability_traits<named_capability> {
  static constexpr std::string_view name = "caors.test.meta.named_capability";
  static constexpr guid stable_guid = guid::from_bytes(guid::byte_array{
      0x2b,
      0x69,
      0x43,
      0x4a,
      0x4f,
      0x5c,
      0x42,
      0x17,
      0x88,
      0x2c,
      0x5f,
      0xe4,
      0x30,
      0x7a,
      0x9a,
      0x31,
  });
  static constexpr std::uint32_t version = 3;
};

} // namespace caors::meta

namespace caors::meta::capabilities {

struct first_extension_module {};
struct second_extension_module {};

template <> struct static_capability<static_capability_data, static_capability_type> {
  [[nodiscard]] static const static_capability_data* get() {
    static const static_capability_data data{42};
    return &data;
  }
};

template <>
struct module_static_capability_list<custom_static_capability_module, static_capability_type> {
  using type = type_list<static_capability_data>;
};

template <> struct static_capability<first_extension_capability, extension_capability_type> {
  [[nodiscard]] static const first_extension_capability* get() {
    static const first_extension_capability data{11};
    return &data;
  }
};

template <> struct static_capability<second_extension_capability, extension_capability_type> {
  [[nodiscard]] static const second_extension_capability* get() {
    static const second_extension_capability data{22};
    return &data;
  }
};

template <>
struct module_static_capability_list<first_extension_module, extension_capability_type> {
  using type = type_list<first_extension_capability>;
};

template <>
struct module_static_capability_list<second_extension_module, extension_capability_type> {
  using type = type_list<second_extension_capability>;
};

template <> struct extension_static_capability_modules<extension_capability_type> {
  using type = type_list<first_extension_module, second_extension_module>;
};

} // namespace caors::meta::capabilities

TEST_CASE("meta - capability info exposes optional stable metadata", "[meta]") {
  const caors::meta::capability_info info = caors::meta::capability_info_of<named_capability>();

  CHECK(info.valid());
  CHECK(info.key == caors::meta::capability_key_of<named_capability>());
  CHECK(info.name == "caors.test.meta.named_capability");
  CHECK(info.stable_guid);
  CHECK(info.version == 3);
}

TEST_CASE("meta - capability registry stores runtime capabilities by type id", "[meta]") {
  static const runtime_capability capability{7};
  const caors::type_id id = caors::meta::id_of<runtime_capability_type>();

  CHECK(caors::meta::capabilities::find<runtime_capability>(id) == nullptr);
  REQUIRE(caors::meta::type_<runtime_capability_type>()
              .capability<runtime_capability>(&capability)
              .succeeded());

  const runtime_capability* found = caors::meta::capabilities::find<runtime_capability>(id);
  REQUIRE(found != nullptr);
  CHECK(found == &capability);
  CHECK(found->value == 7);
  CHECK(caors::meta::capabilities::contains<runtime_capability>(id));

  const caors::meta::type type = caors::meta::find<runtime_capability_type>();
  CHECK(type.get_capability<runtime_capability>() == &capability);
  CHECK(type.has_capability<runtime_capability>());
}

TEST_CASE("meta - capability static provider can be registered into runtime index", "[meta]") {
  const static_capability_data* capability =
      caors::meta::capabilities::get<static_capability_data, static_capability_type>();
  REQUIRE(capability != nullptr);
  CHECK(capability->value == 42);

  REQUIRE(caors::meta::type_<static_capability_type>()
              .capability<static_capability_data>()
              .succeeded());

  const static_capability_data* found = caors::meta::capabilities::find<static_capability_data>(
      caors::meta::id_of<static_capability_type>());
  REQUIRE(found != nullptr);
  CHECK(found == capability);
}

TEST_CASE("meta - type view resolves descriptor static capabilities", "[meta]") {
  const caors::meta::type type = caors::meta::find<static_capability_type>();
  const static_capability_data* found = type.get_capability<static_capability_data>();
  REQUIRE(found != nullptr);
  CHECK(found->value == 42);
  CHECK(type.has_capability<static_capability_data>());
}

TEST_CASE("meta - static capability modules can be extended by module tag list", "[meta]") {
  const caors::meta::type type = caors::meta::find<extension_capability_type>();

  const first_extension_capability* first = type.get_capability<first_extension_capability>();
  REQUIRE(first != nullptr);
  CHECK(first->value == 11);

  const second_extension_capability* second = type.get_capability<second_extension_capability>();
  REQUIRE(second != nullptr);
  CHECK(second->value == 22);
}
