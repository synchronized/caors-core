// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/plugins/abi.h"
#include "caors/plugins/meta_registration.h"
#include "caors/registration.h"

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
    nullptr,
};

} // namespace

extern "C" CAORS_PLUGIN_EXPORT const caors_plugin_entry* CAORS_PLUGIN_CALL
caors_query_plugin(const caors_plugin_host* host, uint32_t) {
  if (host != nullptr) {
    instance.host = *host;
  }
  return &entry;
}
