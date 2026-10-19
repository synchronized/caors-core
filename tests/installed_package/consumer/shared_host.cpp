// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/version.h"

#if defined(_WIN32)
#define CAORS_CONSUMER_EXPORT __declspec(dllexport)
#else
#define CAORS_CONSUMER_EXPORT __attribute__((visibility("default")))
#endif
extern "C" CAORS_CONSUMER_EXPORT int caors_host_version() { return caors::get_version().minor; }
