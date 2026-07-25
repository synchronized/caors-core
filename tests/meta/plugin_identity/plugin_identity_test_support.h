// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_META_PLUGIN_IDENTITY_TEST_SUPPORT_H_
#define CAORS_TESTS_META_PLUGIN_IDENTITY_TEST_SUPPORT_H_

#include <cstdint>

namespace caors::tests::plugin_identity {

struct shared_plugin_type final {
  int value = 0;
};

using type_id_fn = std::uint64_t (*)() noexcept;

} // namespace caors::tests::plugin_identity

#if defined(_WIN32)
#define CAORS_TEST_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define CAORS_TEST_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#endif
