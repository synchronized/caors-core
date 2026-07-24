// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "cross_module_test_support.h"

#include "caors/meta/detail/type_access.h"

#include <vector>

namespace caors::tests::cross_module {

const void* static_type_record_from_support_tu() noexcept {
  return meta::detail::type_access::data_of(meta::resolve<cross_module_static_type>());
}

const void* sequence_ops_from_support_tu() noexcept {
  return meta::resolve<std::vector<int>>().get_raw_ops<meta::sequence_ops>();
}

const void* codec_ops_from_support_tu() noexcept {
  return meta::resolve<std::string>().get_raw_ops<serialization::codec_ops>();
}

} // namespace caors::tests::cross_module
