// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_ADAPTERS_CONTAINER_TRAITS_H_
#define CAORS_ADAPTERS_CONTAINER_TRAITS_H_

#include "caors/traits/container.h"

namespace caors::adapters {

template <typename T> using sequence_traits = traits::sequence_traits<T>;
template <typename T> using set_traits = traits::set_traits<T>;
template <typename T> using associative_traits = traits::associative_traits<T>;
template <typename T> using pair_traits = traits::pair_traits<T>;
template <typename T> using tuple_traits = traits::tuple_traits<T>;
template <typename T> using variant_traits = traits::variant_traits<T>;

} // namespace caors::adapters

#endif
