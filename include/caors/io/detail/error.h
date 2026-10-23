// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors
#ifndef CAORS_IO_DETAIL_ERROR_H_
#define CAORS_IO_DETAIL_ERROR_H_
#include "caors/io/async_request.h"
namespace caors::io::detail {
inline io_error convert_error(error status) noexcept {
  if (!status)
    return {};
  switch (status.value()) {
  case errc::invalid_argument:
    return {io_errc::invalid_argument};
  case errc::not_found:
    return {io_errc::not_found};
  case errc::unsupported:
    return {io_errc::unsupported};
  case errc::cancelled:
    return {io_errc::cancelled};
  case errc::out_of_memory:
    return {io_errc::out_of_memory};
  default:
    return {io_errc::backend_failure};
  }
}
} // namespace caors::io::detail
#endif
