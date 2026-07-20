// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_OBJECT_ID_H_
#define CAORS_OBJECT_ID_H_

#include "caors/meta/object_id.h"

namespace caors::objects {

using object_id = meta::object_id;
using object_id_hash = meta::object_id_hash;
using object_type_id = meta::object_type_id;
using object_guid = meta::object_guid;
using object_guid_hash = meta::object_guid_hash;
using meta::generate_object_id;

} // namespace caors::objects

#endif
