// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_TESTS_META_PLUGIN_IDENTITY_TEST_SUPPORT_H_
#define CAORS_TESTS_META_PLUGIN_IDENTITY_TEST_SUPPORT_H_

#include <cstdint>

namespace caors::tests::plugin_identity {

struct shared_plugin_type final {
  int value = 0;
};

struct consistent_plugin_type final {
  int value = 0;
};

using type_id_fn = std::uint64_t (*)() noexcept;
using register_type_fn = int (*)() noexcept;
using capability_key_fn = std::uint64_t (*)() noexcept;

struct conflicting_static_capability final {
  int marker = 0;
};

struct shared_static_capability final {
  int marker = 0;
};

} // namespace caors::tests::plugin_identity

#if defined(_WIN32)
#define CAORS_TEST_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define CAORS_TEST_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#endif
