// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/value/view_factory.h"

#include "caors/meta/detail/type_descriptor.h"

#include <utility>

namespace caors::value {
namespace {

template <typename Capability>
[[nodiscard]] bool has_capability(const meta::detail::type_descriptor* descriptor) noexcept {
  return meta::detail::descriptor_capability<Capability>(descriptor) != nullptr;
}

[[nodiscard]] bool
has_indirect_capability(const meta::detail::type_descriptor* descriptor) noexcept {
  const meta::detail::indirect_data* indirect =
      meta::detail::descriptor_capability<meta::detail::indirect_data>(descriptor);
  return descriptor != nullptr && meta::detail::descriptor_indirect(*descriptor) &&
         indirect != nullptr && indirect->valid();
}

} // namespace

bool indirect_view::valid() const noexcept {
  const meta::detail::indirect_data* info = data();
  if (descriptor_ == nullptr || info == nullptr || storage_ == nullptr ||
      !meta::detail::descriptor_value_type(*descriptor_)) {
    return false;
  }

  if (!meta::detail::descriptor_indirect(*descriptor_)) {
    return false;
  }

  if (meta::detail::descriptor_kind(*descriptor_) == meta::type_kind::wrapper &&
      meta::detail::descriptor_wrapper_semantics(*descriptor_) ==
          meta::wrapper_semantics::unsupported) {
    return false;
  }

  return info->valid();
}

type_id indirect_view::value_type() const noexcept {
  return valid() ? meta::detail::descriptor_value_type(*descriptor_) : type_id{};
}

string_name indirect_view::value_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(value_type()) : meta::type{};
  return meta ? meta.name() : string_name{};
}

bool indirect_view::has_value() const noexcept {
  const meta::detail::indirect_data* info = data();
  if (!valid() || info->has_value == nullptr) {
    return false;
  }

  return info->has_value(storage_);
}

any_ref indirect_view::value_ref() const {
  const meta::detail::indirect_data* info = data();
  if (!valid() || info->value == nullptr) {
    return {};
  }

  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && info->mutable_value != nullptr) {
    value = info->mutable_value(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else {
    value = info->value(storage_);
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(value_type(), info->value_descriptor, value, value_writable);
}

bool indirect_view::reset() const {
  const meta::detail::indirect_data* info = data();
  if (!writable() || info->reset == nullptr) {
    return false;
  }

  return info->reset(const_cast<void*>(storage_));
}

bool indirect_view::ensure_value() const {
  const meta::detail::indirect_data* info = data();
  if (!writable() || info->ensure_value == nullptr) {
    return false;
  }

  return info->ensure_value(const_cast<void*>(storage_));
}

indirect_view::indirect_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                             bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

const meta::detail::indirect_data* indirect_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::indirect_data>(descriptor_);
}

bool pair_view::valid() const noexcept {
  const meta::detail::pair_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

type_id pair_view::first_type() const noexcept {
  const meta::detail::pair_data* info = data();
  return valid() ? info->key_type : type_id{};
}

string_name pair_view::first_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->key_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

type_id pair_view::value_type() const noexcept {
  const meta::detail::pair_data* info = data();
  return valid() ? info->value_type : type_id{};
}

string_name pair_view::value_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

any_ref pair_view::first_ref() const {
  const meta::detail::pair_data* info = data();
  if (!valid() || info->first == nullptr) {
    return {};
  }

  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && info->mutable_first != nullptr) {
    value = info->mutable_first(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else {
    value = info->first(storage_);
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(info->key_type, info->key_descriptor, value, value_writable);
}

any_ref pair_view::second_ref() const {
  const meta::detail::pair_data* info = data();
  if (!valid() || info->second == nullptr) {
    return {};
  }

  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && info->mutable_second != nullptr) {
    value = info->mutable_second(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else {
    value = info->second(storage_);
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(info->value_type, info->value_descriptor, value, value_writable);
}

bool pair_view::set_first(const any& value) const {
  const meta::detail::pair_data* info = data();
  if (!writable() || !value.valid() || value.type() != info->key_type) {
    return false;
  }

  any_ref target = first_ref();
  return target.valid() && target.try_set(value);
}

bool pair_view::set_second(const any& value) const {
  const meta::detail::pair_data* info = data();
  if (!writable() || !value.valid() || value.type() != info->value_type) {
    return false;
  }

  any_ref target = second_ref();
  return target.valid() && target.try_set(value);
}

pair_view::pair_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                     bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

const meta::detail::pair_data* pair_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::pair_data>(descriptor_);
}

bool tuple_view::valid() const noexcept {
  const meta::detail::tuple_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

std::size_t tuple_view::size() const noexcept {
  const meta::detail::tuple_data* info = data();
  return valid() ? info->elements.size() : 0;
}

std::optional<type_id> tuple_view::element_type(std::size_t index) const noexcept {
  const meta::detail::tuple_data* info = data();
  if (!valid() || index >= info->elements.size()) {
    return std::nullopt;
  }

  return info->elements[index].value_type;
}

string_name tuple_view::element_type_name(std::size_t index) const noexcept {
  const std::optional<type_id> id = element_type(index);
  const meta::type meta = id.has_value() ? caors::meta::find(*id) : meta::type{};
  return meta ? meta.name() : string_name{};
}

any_ref tuple_view::element_ref(std::size_t index) const {
  const meta::detail::tuple_data* info = data();
  if (!valid() || index >= info->elements.size()) {
    return {};
  }

  const meta::detail::tuple_element_data& element_data = info->elements[index];
  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && element_data.mutable_element != nullptr) {
    value = element_data.mutable_element(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else if (element_data.element != nullptr) {
    value = element_data.element(storage_);
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(element_data.value_type, element_data.value_descriptor, value,
                           value_writable);
}

bool tuple_view::set_element(std::size_t index, const any& value) const {
  const std::optional<type_id> expected = element_type(index);
  if (!writable() || !expected.has_value() || !value.valid() || value.type() != *expected) {
    return false;
  }

  any_ref target = element_ref(index);
  return target.valid() && target.try_set(value);
}

tuple_view::tuple_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                       bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

const meta::detail::tuple_data* tuple_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::tuple_data>(descriptor_);
}

bool variant_view::valid() const noexcept {
  const meta::detail::variant_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

std::size_t variant_view::size() const noexcept {
  const meta::detail::variant_data* info = data();
  return valid() ? info->alternatives.size() : 0;
}

std::optional<std::size_t> variant_view::index() const noexcept {
  const meta::detail::variant_data* info = data();
  if (!valid() || info->index == nullptr) {
    return std::nullopt;
  }

  const std::size_t active = info->index(storage_);
  if (active >= info->alternatives.size()) {
    return std::nullopt;
  }

  return active;
}

std::optional<type_id> variant_view::alternative_type(std::size_t alternative) const noexcept {
  const meta::detail::variant_data* info = data();
  if (!valid() || alternative >= info->alternatives.size()) {
    return std::nullopt;
  }

  return info->alternatives[alternative].value_type;
}

string_name variant_view::alternative_type_name(std::size_t alternative) const noexcept {
  const std::optional<type_id> id = alternative_type(alternative);
  const meta::type meta = id.has_value() ? caors::meta::find(*id) : meta::type{};
  return meta ? meta.name() : string_name{};
}

type_id variant_view::value_type() const noexcept {
  const std::optional<std::size_t> active = index();
  if (!active.has_value()) {
    return {};
  }

  return data()->alternatives[*active].value_type;
}

string_name variant_view::value_type_name() const noexcept {
  const meta::type meta = caors::meta::find(value_type());
  return meta ? meta.name() : string_name{};
}

any_ref variant_view::value_ref() const {
  const std::optional<std::size_t> active = index();
  if (!active.has_value()) {
    return {};
  }

  const meta::detail::variant_alternative_data& alternative_data = data()->alternatives[*active];
  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && alternative_data.mutable_alternative != nullptr) {
    value = alternative_data.mutable_alternative(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else if (alternative_data.alternative != nullptr) {
    value = alternative_data.alternative(storage_);
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(alternative_data.value_type, alternative_data.value_descriptor, value,
                           value_writable);
}

bool variant_view::emplace(std::size_t alternative) const {
  const meta::detail::variant_data* info = data();
  if (!writable() || alternative >= info->alternatives.size()) {
    return false;
  }

  const std::optional<std::size_t> active = index();
  if (active.has_value() && *active == alternative) {
    return true;
  }

  const meta::detail::variant_alternative_data& alternative_data = info->alternatives[alternative];
  if (alternative_data.emplace == nullptr) {
    return false;
  }

  return alternative_data.emplace(const_cast<void*>(storage_));
}

bool variant_view::set_value(const any& value) const {
  const type_id expected = value_type();
  if (!writable() || !expected || !value.valid() || value.type() != expected) {
    return false;
  }

  any_ref target = value_ref();
  return target.valid() && target.try_set(value);
}

variant_view::variant_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                           bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

const meta::detail::variant_data* variant_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::variant_data>(descriptor_);
}

any_ref sequence_view::iterator::operator*() const {
  return view_ != nullptr ? view_->element_ref(index_) : any_ref{};
}

sequence_view::iterator& sequence_view::iterator::operator++() noexcept {
  ++index_;
  return *this;
}

sequence_view::iterator sequence_view::iterator::operator++(int) noexcept {
  iterator previous = *this;
  ++(*this);
  return previous;
}

bool operator==(sequence_view::iterator lhs, sequence_view::iterator rhs) noexcept {
  return lhs.view_ == rhs.view_ && lhs.index_ == rhs.index_;
}

bool operator!=(sequence_view::iterator lhs, sequence_view::iterator rhs) noexcept {
  return !(lhs == rhs);
}

sequence_view::iterator::iterator(const sequence_view* view, std::size_t index) noexcept
    : view_(view), index_(index) {}

bool sequence_view::valid() const noexcept {
  const meta::detail::sequence_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

type_id sequence_view::value_type() const noexcept {
  const meta::detail::sequence_data* info = data();
  return valid() ? info->value_type : type_id{};
}

string_name sequence_view::value_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

std::optional<std::size_t> sequence_view::size() const {
  const meta::detail::sequence_data* info = data();
  if (!valid() || info->size == nullptr) {
    return std::nullopt;
  }

  return info->size(storage_);
}

sequence_view::iterator sequence_view::begin() const {
  return valid() ? iterator{this, 0} : iterator{};
}

sequence_view::iterator sequence_view::end() const {
  const std::optional<std::size_t> count = size();
  return count.has_value() ? iterator{this, *count} : iterator{};
}

any_ref sequence_view::element_ref(std::size_t index) const {
  const meta::detail::sequence_data* info = data();
  if (!valid() || info->element == nullptr) {
    return {};
  }

  const void* element = nullptr;
  bool element_writable = false;
  if (writable() && info->mutable_element != nullptr) {
    element = info->mutable_element(const_cast<void*>(storage_), index);
    element_writable = element != nullptr;
  } else {
    element = info->element(storage_, index);
  }

  if (element == nullptr) {
    return {};
  }

  return any_ref::from_raw(info->value_type, info->value_descriptor, element, element_writable);
}

bool sequence_view::clear() const {
  const meta::detail::sequence_data* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool sequence_view::resize(std::size_t size) const {
  const meta::detail::sequence_data* info = data();
  if (!writable() || info->resize == nullptr) {
    return false;
  }

  return info->resize(const_cast<void*>(storage_), size);
}

bool sequence_view::append_value(const any& value) const {
  const meta::detail::sequence_data* info = data();
  if (!writable() || info->append == nullptr || !value.valid() ||
      value.type() != info->value_type) {
    return false;
  }

  return info->append(const_cast<void*>(storage_), value.data());
}

sequence_view::sequence_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                             bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

const meta::detail::sequence_data* sequence_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::sequence_data>(descriptor_);
}

set_view::iterator::iterator(const iterator& other) : view_(other.view_) {
  const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (other.iterator_ != nullptr && info != nullptr && info->iterator_clone != nullptr) {
    iterator_ = info->iterator_clone(other.iterator_);
  }
}

set_view::iterator::iterator(iterator&& other) noexcept
    : view_(other.view_), iterator_(other.iterator_) {
  other.view_ = nullptr;
  other.iterator_ = nullptr;
}

set_view::iterator::~iterator() { reset(); }

set_view::iterator& set_view::iterator::operator=(const iterator& other) {
  if (this == &other) {
    return *this;
  }

  iterator copy{other};
  swap(copy);
  return *this;
}

set_view::iterator& set_view::iterator::operator=(iterator&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  reset();
  view_ = other.view_;
  iterator_ = other.iterator_;
  other.view_ = nullptr;
  other.iterator_ = nullptr;
  return *this;
}

any_ref set_view::iterator::operator*() const {
  const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info == nullptr || iterator_ == nullptr || info->iterator_value == nullptr) {
    return {};
  }

  const void* value = info->iterator_value(iterator_);
  return any_ref::from_raw(info->value_type, info->value_descriptor, value, false);
}

set_view::iterator& set_view::iterator::operator++() noexcept {
  const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator_advance != nullptr) {
    info->iterator_advance(iterator_);
  }

  return *this;
}

set_view::iterator set_view::iterator::operator++(int) {
  iterator previous = *this;
  ++(*this);
  return previous;
}

bool operator==(const set_view::iterator& lhs, const set_view::iterator& rhs) noexcept {
  if (lhs.view_ != rhs.view_) {
    return lhs.iterator_ == nullptr && rhs.iterator_ == nullptr;
  }

  if (lhs.iterator_ == nullptr || rhs.iterator_ == nullptr) {
    return true;
  }

  const meta::detail::set_data* info = lhs.view_ != nullptr ? lhs.view_->data() : nullptr;
  if (info == nullptr || info->iterator_equal == nullptr) {
    return false;
  }

  return info->iterator_equal(lhs.iterator_, rhs.iterator_);
}

bool operator!=(const set_view::iterator& lhs, const set_view::iterator& rhs) noexcept {
  return !(lhs == rhs);
}

set_view::iterator::iterator(const set_view* view, void* iterator) noexcept
    : view_(view), iterator_(iterator) {}

void set_view::iterator::swap(iterator& other) noexcept {
  using std::swap;
  swap(view_, other.view_);
  swap(iterator_, other.iterator_);
}

void set_view::iterator::reset() noexcept {
  const meta::detail::set_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator_destroy != nullptr) {
    info->iterator_destroy(iterator_);
  }

  view_ = nullptr;
  iterator_ = nullptr;
}

bool set_view::valid() const noexcept {
  const meta::detail::set_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

type_id set_view::value_type() const noexcept {
  const meta::detail::set_data* info = data();
  return valid() ? info->value_type : type_id{};
}

string_name set_view::value_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

std::optional<std::size_t> set_view::size() const {
  const meta::detail::set_data* info = data();
  if (!valid() || info->size == nullptr) {
    return std::nullopt;
  }

  return info->size(storage_);
}

set_view::iterator set_view::begin() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->begin(storage_)};
}

set_view::iterator set_view::end() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->end(storage_)};
}

bool set_view::for_each_ref(set_element_visitor visitor, void* user_data) const {
  const meta::detail::set_data* info = data();
  if (!valid() || info->visit == nullptr || visitor == nullptr) {
    return false;
  }

  struct visit_context {
    const meta::detail::set_data* data = nullptr;
    set_element_visitor visitor = nullptr;
    void* user_data = nullptr;
  };

  visit_context context{info, visitor, user_data};
  return info->visit(
      storage_,
      [](const void* element, void* raw_context) -> bool {
        auto* context = static_cast<visit_context*>(raw_context);
        const meta::detail::set_data* data = context->data;
        any_ref element_ref =
            any_ref::from_raw(data->value_type, data->value_descriptor, element, false);

        return context->visitor(element_ref, context->user_data);
      },
      &context);
}

bool set_view::clear() const {
  const meta::detail::set_data* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool set_view::insert_value(const any& value) const {
  const meta::detail::set_data* info = data();
  if (!writable() || info->insert == nullptr || !value.valid() ||
      value.type() != info->value_type) {
    return false;
  }

  return info->insert(const_cast<void*>(storage_), value.data());
}

set_view::set_view(const meta::detail::type_descriptor* descriptor, const void* storage,
                   bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

bool set_view::has_iterator_support() const noexcept {
  const meta::detail::set_data* info = data();
  return valid() && info->begin != nullptr && info->end != nullptr &&
         info->iterator_clone != nullptr && info->iterator_destroy != nullptr &&
         info->iterator_advance != nullptr && info->iterator_equal != nullptr &&
         info->iterator_value != nullptr;
}

const meta::detail::set_data* set_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::set_data>(descriptor_);
}

associative_view::iterator::iterator(const iterator& other) : view_(other.view_) {
  const meta::detail::associative_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (other.iterator_ != nullptr && info != nullptr && info->iterator_clone != nullptr) {
    iterator_ = info->iterator_clone(other.iterator_);
  }
}

associative_view::iterator::iterator(iterator&& other) noexcept
    : view_(other.view_), iterator_(other.iterator_) {
  other.view_ = nullptr;
  other.iterator_ = nullptr;
}

associative_view::iterator::~iterator() { reset(); }

associative_view::iterator& associative_view::iterator::operator=(const iterator& other) {
  if (this == &other) {
    return *this;
  }

  iterator copy{other};
  swap(copy);
  return *this;
}

associative_view::iterator& associative_view::iterator::operator=(iterator&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  reset();
  view_ = other.view_;
  iterator_ = other.iterator_;
  other.view_ = nullptr;
  other.iterator_ = nullptr;
  return *this;
}

associative_entry_ref associative_view::iterator::operator*() const {
  const meta::detail::associative_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info == nullptr || iterator_ == nullptr || info->iterator_key == nullptr ||
      info->iterator_value == nullptr) {
    return {};
  }

  const void* key = info->iterator_key(iterator_);
  const void* value = info->iterator_value(iterator_);
  return associative_entry_ref{
      any_ref::from_raw(info->key_type, info->key_descriptor, key, false),
      any_ref::from_raw(info->value_type, info->value_descriptor, value, false),
  };
}

associative_view::iterator& associative_view::iterator::operator++() noexcept {
  const meta::detail::associative_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator_advance != nullptr) {
    info->iterator_advance(iterator_);
  }

  return *this;
}

associative_view::iterator associative_view::iterator::operator++(int) {
  iterator previous = *this;
  ++(*this);
  return previous;
}

bool operator==(const associative_view::iterator& lhs,
                const associative_view::iterator& rhs) noexcept {
  if (lhs.view_ != rhs.view_) {
    return lhs.iterator_ == nullptr && rhs.iterator_ == nullptr;
  }

  if (lhs.iterator_ == nullptr || rhs.iterator_ == nullptr) {
    return true;
  }

  const meta::detail::associative_data* info = lhs.view_ != nullptr ? lhs.view_->data() : nullptr;
  if (info == nullptr || info->iterator_equal == nullptr) {
    return false;
  }

  return info->iterator_equal(lhs.iterator_, rhs.iterator_);
}

bool operator!=(const associative_view::iterator& lhs,
                const associative_view::iterator& rhs) noexcept {
  return !(lhs == rhs);
}

associative_view::iterator::iterator(const associative_view* view, void* iterator) noexcept
    : view_(view), iterator_(iterator) {}

void associative_view::iterator::swap(iterator& other) noexcept {
  using std::swap;
  swap(view_, other.view_);
  swap(iterator_, other.iterator_);
}

void associative_view::iterator::reset() noexcept {
  const meta::detail::associative_data* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator_destroy != nullptr) {
    info->iterator_destroy(iterator_);
  }

  view_ = nullptr;
  iterator_ = nullptr;
}

bool associative_view::valid() const noexcept {
  const meta::detail::associative_data* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

type_id associative_view::key_type() const noexcept {
  const meta::detail::associative_data* info = data();
  return valid() ? info->key_type : type_id{};
}

string_name associative_view::key_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->key_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

type_id associative_view::value_type() const noexcept {
  const meta::detail::associative_data* info = data();
  return valid() ? info->value_type : type_id{};
}

string_name associative_view::value_type_name() const noexcept {
  const meta::type meta = valid() ? caors::meta::find(data()->value_type) : meta::type{};
  return meta ? meta.name() : string_name{};
}

std::optional<std::size_t> associative_view::size() const {
  const meta::detail::associative_data* info = data();
  if (!valid() || info->size == nullptr) {
    return std::nullopt;
  }

  return info->size(storage_);
}

associative_view::iterator associative_view::begin() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->begin(storage_)};
}

associative_view::iterator associative_view::end() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->end(storage_)};
}

bool associative_view::for_each_ref(associative_entry_visitor visitor, void* user_data) const {
  const meta::detail::associative_data* info = data();
  if (!valid() || info->visit == nullptr || visitor == nullptr) {
    return false;
  }

  struct visit_context {
    const meta::detail::associative_data* data = nullptr;
    associative_entry_visitor visitor = nullptr;
    void* user_data = nullptr;
  };

  visit_context context{info, visitor, user_data};
  return info->visit(
      storage_,
      [](const void* key, const void* mapped, void* raw_context) -> bool {
        auto* context = static_cast<visit_context*>(raw_context);
        const meta::detail::associative_data* data = context->data;
        any_ref key_ref = any_ref::from_raw(data->key_type, data->key_descriptor, key, false);
        any_ref mapped_ref =
            any_ref::from_raw(data->value_type, data->value_descriptor, mapped, false);

        return context->visitor(key_ref, mapped_ref, context->user_data);
      },
      &context);
}

bool associative_view::clear() const {
  const meta::detail::associative_data* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool associative_view::insert_value(const any& key, const any& mapped) const {
  const meta::detail::associative_data* info = data();
  if (!writable() || info->insert == nullptr || !key.valid() || !mapped.valid() ||
      key.type() != info->key_type || mapped.type() != info->value_type) {
    return false;
  }

  return info->insert(const_cast<void*>(storage_), key.data(), mapped.data());
}

associative_view::associative_view(const meta::detail::type_descriptor* descriptor,
                                   const void* storage, bool writable) noexcept
    : descriptor_(descriptor), storage_(storage), writable_(writable) {}

bool associative_view::has_iterator_support() const noexcept {
  const meta::detail::associative_data* info = data();
  return valid() && info->begin != nullptr && info->end != nullptr &&
         info->iterator_clone != nullptr && info->iterator_destroy != nullptr &&
         info->iterator_advance != nullptr && info->iterator_equal != nullptr &&
         info->iterator_key != nullptr && info->iterator_value != nullptr;
}

const meta::detail::associative_data* associative_view::data() const noexcept {
  return meta::detail::descriptor_capability<meta::detail::associative_data>(descriptor_);
}

associative_view any::create_associative_view() {
  if (!valid() || !has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, mutable_data(), true};
}

associative_view any::create_associative_view() const {
  if (!valid() || !has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, data_, false};
}

associative_view any_ref::create_associative_view() const {
  if (!valid() || !has_capability<meta::detail::associative_data>(descriptor_)) {
    return {};
  }

  return associative_view{descriptor_, data_, writable_};
}

pair_view any::create_pair_view() {
  if (!valid() || !has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, mutable_data(), true};
}

pair_view any::create_pair_view() const {
  if (!valid() || !has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, data_, false};
}

pair_view any_ref::create_pair_view() const {
  if (!valid() || !has_capability<meta::detail::pair_data>(descriptor_)) {
    return {};
  }

  return pair_view{descriptor_, data_, writable_};
}

tuple_view any::create_tuple_view() {
  if (!valid() || !has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, mutable_data(), true};
}

tuple_view any::create_tuple_view() const {
  if (!valid() || !has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, data_, false};
}

tuple_view any_ref::create_tuple_view() const {
  if (!valid() || !has_capability<meta::detail::tuple_data>(descriptor_)) {
    return {};
  }

  return tuple_view{descriptor_, data_, writable_};
}

variant_view any::create_variant_view() {
  if (!valid() || !has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, mutable_data(), true};
}

variant_view any::create_variant_view() const {
  if (!valid() || !has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, data_, false};
}

variant_view any_ref::create_variant_view() const {
  if (!valid() || !has_capability<meta::detail::variant_data>(descriptor_)) {
    return {};
  }

  return variant_view{descriptor_, data_, writable_};
}

indirect_view any::create_indirect_view() {
  if (!valid() || !has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, mutable_data(), true};
}

indirect_view any::create_indirect_view() const {
  if (!valid() || !has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, data_, false};
}

indirect_view any_ref::create_indirect_view() const {
  if (!valid() || !has_indirect_capability(descriptor_)) {
    return {};
  }

  return indirect_view{descriptor_, data_, writable_};
}

sequence_view any::create_sequence_view() {
  if (!valid() || !has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, mutable_data(), true};
}

sequence_view any::create_sequence_view() const {
  if (!valid() || !has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, data_, false};
}

sequence_view any_ref::create_sequence_view() const {
  if (!valid() || !has_capability<meta::detail::sequence_data>(descriptor_)) {
    return {};
  }

  return sequence_view{descriptor_, data_, writable_};
}

set_view any::create_set_view() {
  if (!valid() || !has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, mutable_data(), true};
}

set_view any::create_set_view() const {
  if (!valid() || !has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, data_, false};
}

set_view any_ref::create_set_view() const {
  if (!valid() || !has_capability<meta::detail::set_data>(descriptor_)) {
    return {};
  }

  return set_view{descriptor_, data_, writable_};
}

} // namespace caors::value
