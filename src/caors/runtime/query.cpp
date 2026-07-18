// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/runtime/query.h"

namespace caors::runtime {

type find(type_id id) { return meta::find(id); }

type find(type_guid guid) { return meta::find(guid); }

type find(std::string_view name) { return meta::find(name); }

type find(string_name name) { return meta::find(name); }

} // namespace caors::runtime
