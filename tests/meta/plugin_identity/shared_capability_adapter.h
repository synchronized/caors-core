// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_META_PLUGIN_IDENTITY_SHARED_CAPABILITY_ADAPTER_H_
#define CAORS_TESTS_META_PLUGIN_IDENTITY_SHARED_CAPABILITY_ADAPTER_H_

#include "plugin_identity_test_support.h"

#include "caors/meta/static_capability.h"

namespace caors::meta::capabilities {

template <>
struct static_capability<::caors::tests::plugin_identity::shared_static_capability,
                         ::caors::tests::plugin_identity::consistent_plugin_type> {
  [[nodiscard]] static const ::caors::tests::plugin_identity::shared_static_capability* get() {
    static const ::caors::tests::plugin_identity::shared_static_capability capability{7};
    return &capability;
  }
};

template <>
struct module_static_capability_list<custom_static_capability_module,
                                     ::caors::tests::plugin_identity::consistent_plugin_type> {
  using type = type_list<::caors::tests::plugin_identity::shared_static_capability>;
};

} // namespace caors::meta::capabilities

#endif
