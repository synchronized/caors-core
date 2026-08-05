// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"
#include "caors/plugins/meta_registration.h"
#include "caors/registration.h"
#include "caors/serialization/codec.h"
#include "caors/serialization/types.h"

#include <cstring>
#include <string_view>

namespace {

struct second_plugin_type {
  int value = 24;
};

struct plugin_instance {
  caors_plugin_host host{};
};

plugin_instance instance{};

caors::serialization::variant_result make_value(const caors::serialization::context& context,
                                                const caors::meta::any_ref& source) {
  const auto* value = source.try_get<second_plugin_type>();
  if (value == nullptr) {
    return caors::fail<caors::variant>(
        {caors::error_domain::serialization, caors::errc::invalid_type});
  }
  return caors::ok(
      caors::variant::from(std::string_view{"second-plugin-result-requires-owned-storage"},
                           context.effective_result_resource()));
}

const caors::serialization::codec_ops codec{nullptr, nullptr, nullptr, nullptr, &make_value};

int32_t CAORS_PLUGIN_CALL initialize(void* data) {
  auto& self = *static_cast<plugin_instance*>(data);
  caors::meta::registry* registry = caors::plugins::find_registration_registry(&self.host, data);
  if (registry == nullptr) {
    return 1;
  }
  caors::meta::context context{*registry};
  return caors::registration::type_<second_plugin_type>(context, "caors.test.plugin.second")
                 .meta()
                 .property("value", &second_plugin_type::value)
                 .capability(codec)
                 .serialization()
                 .field("value")
                 .commit()
             ? 1
             : 0;
}

int32_t CAORS_PLUGIN_CALL succeed(void*) { return 0; }

const void* CAORS_PLUGIN_CALL get_extension(void*, const char* id, size_t id_size,
                                            uint32_t version) {
  static const caors_plugin_build_extension build{sizeof(caors_plugin_build_extension),
                                                  CAORS_PLUGIN_BUILD_EXTENSION_VERSION,
                                                  CAORS_PLUGIN_BUILD_ENVIRONMENT_INITIALIZER};
  return version == CAORS_PLUGIN_BUILD_EXTENSION_VERSION &&
                 id_size == sizeof(CAORS_PLUGIN_BUILD_EXTENSION_ID) - 1 &&
                 std::memcmp(id, CAORS_PLUGIN_BUILD_EXTENSION_ID, id_size) == 0
             ? &build
             : nullptr;
}

constexpr char plugin_name[] = "caors.test.second";
constexpr char display_name[] = "CAORS 第二测试插件";
const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor),
    {{0x8f, 0x13, 0x2e, 0x44, 0x9b, 0xd0, 0x4e, 0x8f, 0x81, 0x5a, 0x63, 0x2d, 0x18, 0x77, 0xa4,
      0x21}},
    {plugin_name, sizeof(plugin_name) - 1},
    {display_name, sizeof(display_name) - 1},
    {1, 0, 0},
};
const caors_plugin_entry entry{sizeof(caors_plugin_entry),
                               CAORS_PLUGIN_ABI_VERSION,
                               &descriptor,
                               &instance,
                               &initialize,
                               &succeed,
                               &succeed,
                               &succeed,
                               &get_extension};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t host_abi_version) {
  if (host == nullptr || host_abi_version != CAORS_PLUGIN_ABI_VERSION) {
    return nullptr;
  }
  instance.host = *host;
  return &entry;
}
