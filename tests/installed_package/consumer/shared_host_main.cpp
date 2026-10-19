// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/version.h"

extern "C" int caors_host_version();
int main() { return caors_host_version() == caors::k_version.minor ? 0 : 1; }
