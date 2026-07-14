// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/adapters/stl_traits.h"
#include "caors/meta/value.h"

#include <iostream>
#include <vector>

int main() {
  std::vector<int> values{1, 2, 3};

  const caors::meta::type sequence_type = caors::meta::resolve<std::vector<int>>();
  const caors::meta::sequence_capability sequence =
      sequence_type.get_capability<caors::meta::sequence_capability>();
  if (!sequence) {
    return 1;
  }

  caors::meta::sequence_view view = sequence.bind(caors::meta::any_ref::from(values));
  if (!view || !view.size().has_value()) {
    return 1;
  }

  std::cout << "size: " << *view.size() << '\n';
  for (caors::meta::any_ref element : view) {
    int* value = element.try_get_mutable<int>();
    if (value == nullptr) {
      return 1;
    }

    *value *= 2;
  }

  for (int value : values) {
    std::cout << value << '\n';
  }

  return 0;
}
