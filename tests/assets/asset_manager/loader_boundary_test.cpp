// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/resources/database.h"

// 单独翻译单元验证 资源 database 与 loader 协议不再通过传递包含依赖具体来源或文件系统。
#if defined(CAORS_RESOURCES_SOURCE_H_) || defined(CAORS_IO_FILE_SYSTEM_H_)
#error "resource database must not include VFS source or file_system"
#endif

static_assert(std::is_polymorphic_v<caors::resources::load_snapshot>);

namespace {
class probe_resource : public caors::resources::resource {};
template <typename T>
concept accepts_sync_composition =
    requires(caors::resources::cache<probe_resource>& cache, T& loader) {
      caors::resources::load_handle_into(cache, loader, caors::resources::resource_guid{});
      caors::resources::reload_handle_into(cache, loader, caors::resources::resource_guid{});
    };
static_assert(!accepts_sync_composition<caors::resources::loader<probe_resource>>);
static_assert(accepts_sync_composition<caors::resources::sync_loader_adapter<probe_resource>>);
} // namespace
