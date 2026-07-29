// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"
#include "caors/plugins/meta_registration.h"
#include "caors/registration.h"

#include <cstring>

namespace {

struct failed_draft_type {};

struct failure_instance {
  caors_plugin_host host{};
};

failure_instance instance{};

int32_t CAORS_PLUGIN_CALL fail_initialize(void* data) {
  auto& self = *static_cast<failure_instance*>(data);
  if (caors::meta::registry* registry =
          caors::plugins::find_registration_registry(&self.host, data);
      registry != nullptr) {
    caors::meta::context context{*registry};
    if (caors::registration::type_<failed_draft_type>(context, "caors.test.plugin.failed_draft")
            .commit()) {
      return 1;
    }
  }
  return 1;
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

constexpr char plugin_name[] = "caors.test.failure";
const caors_plugin_descriptor descriptor{
    sizeof(caors_plugin_descriptor), {{2}}, {plugin_name, sizeof(plugin_name) - 1}, {}, {1, 0, 0},
};
const caors_plugin_entry entry{
    sizeof(caors_plugin_entry),
    CAORS_PLUGIN_ABI_VERSION,
    &descriptor,
    &instance,
    &fail_initialize,
    &succeed,
    &succeed,
    &succeed,
    &get_extension,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t) {
  if (host != nullptr) {
    instance.host = *host;
  }
  return &entry;
}
