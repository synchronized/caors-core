// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_MEMORY_DETAIL_INTERNAL_H_
#define CAORS_MEMORY_DETAIL_INTERNAL_H_

#include "caors/memory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace caors::memory::detail {

[[nodiscard]] bool is_power_of_two(std::size_t value) noexcept;
[[nodiscard]] std::size_t normalize_size(std::size_t size) noexcept;
[[noreturn]] void fail_bad_alloc();
[[noreturn]] void fail_invalid_argument(const char* message);
void validate_alignment_or_fail(std::size_t alignment);
[[nodiscard]] bool needs_aligned_new(std::size_t alignment) noexcept;
[[nodiscard]] bool try_checked_add(std::size_t lhs, std::size_t rhs, std::size_t& result) noexcept;
[[nodiscard]] bool try_checked_multiply(std::size_t lhs, std::size_t rhs,
                                        std::size_t& result) noexcept;
[[nodiscard]] bool try_checked_add_address(std::uintptr_t lhs, std::size_t rhs,
                                           std::uintptr_t& result) noexcept;
[[nodiscard]] bool try_align_up(std::size_t value, std::size_t alignment,
                                std::size_t& result) noexcept;
[[nodiscard]] std::size_t align_up_or_fail(std::size_t value, std::size_t alignment);
[[nodiscard]] bool try_align_up_address(std::uintptr_t value, std::size_t alignment,
                                        std::uintptr_t& result) noexcept;
void update_peak(std::atomic<std::size_t>& peak, std::size_t value) noexcept;
void subtract_saturating(std::atomic<std::size_t>& value, std::size_t amount) noexcept;
[[nodiscard]] memory_resource& system_resource_instance() noexcept;
[[nodiscard]] memory_resource& resolve_resource(memory_resource* resource) noexcept;
[[nodiscard]] memory_resource* replace_default_resource(memory_resource* resource) noexcept;

} // namespace caors::memory::detail

#endif
