// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#include "caors/meta/value/view_factory.h"

#include "caors/meta/query.h"
#include "caors/meta/value/any_ref.h"
#include "caors/meta/value/associative_entry_ref.h"

#include <utility>

namespace caors::meta {
namespace {

template <typename Ops> [[nodiscard]] bool has_raw_ops(meta::type type) noexcept {
  return type.get_raw_ops<Ops>() != nullptr;
}

[[nodiscard]] bool has_indirect_ops(meta::type type) noexcept {
  const meta::indirect_ops* indirect = type.get_raw_ops<meta::indirect_ops>();
  return type.indirect() && indirect != nullptr && indirect->valid();
}

} // namespace

indirect_view make_indirect_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_indirect_ops(type)) {
    return {};
  }

  return indirect_view{type, source.data(), source.writable()};
}

sequence_view make_sequence_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::sequence_ops>(type)) {
    return {};
  }

  return sequence_view{type, source.data(), source.writable()};
}

set_view make_set_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::set_ops>(type)) {
    return {};
  }

  return set_view{type, source.data(), source.writable()};
}

associative_view make_associative_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::associative_ops>(type)) {
    return {};
  }

  return associative_view{type, source.data(), source.writable()};
}

pair_view make_pair_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::pair_ops>(type)) {
    return {};
  }

  return pair_view{type, source.data(), source.writable()};
}

tuple_view make_tuple_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::tuple_ops>(type)) {
    return {};
  }

  return tuple_view{type, source.data(), source.writable()};
}

variant_view make_variant_view(any_ref source) noexcept {
  const meta::type type = source.get_type();
  if (!source.valid() || !has_raw_ops<meta::variant_ops>(type)) {
    return {};
  }

  return variant_view{type, source.data(), source.writable()};
}

bool indirect_view::valid() const noexcept {
  const meta::indirect_ops* info = data();
  if (!type_ || info == nullptr || storage_ == nullptr || !type_.get_value_type()) {
    return false;
  }

  if (!type_.indirect()) {
    return false;
  }

  if (type_.kind() == meta::type_kind::wrapper &&
      type_.wrapper() == meta::wrapper_semantics::unsupported) {
    return false;
  }

  return info->valid();
}

meta::type indirect_view::get_value_type() const noexcept {
  return valid() ? type_.get_value_type() : meta::type{};
}

bool indirect_view::has_value() const noexcept {
  const meta::indirect_ops* info = data();
  if (!valid() || info->has_value == nullptr) {
    return false;
  }

  return info->has_value(storage_);
}

any_ref indirect_view::value_ref() const {
  const meta::indirect_ops* info = data();
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

  return any_ref::from_raw(get_value_type(), value, value_writable);
}

meta::type indirect_view::get_dynamic_value_type() const noexcept {
  const meta::indirect_ops* info = data();
  const meta::type declared_type = get_value_type();
  const type_id dynamic_type = !valid() || info == nullptr || info->dynamic_type == nullptr
                                   ? declared_type.id()
                                   : info->dynamic_type(storage_);

  if (dynamic_type == declared_type.id()) {
    return declared_type;
  }

  return caors::meta::find(dynamic_type);
}

any_ref indirect_view::dynamic_value_ref() const {
  const meta::indirect_ops* info = data();
  if (!valid()) {
    return {};
  }

  const type_id dynamic_type = get_dynamic_value_type().id();
  if (!dynamic_type) {
    return {};
  }

  const void* value = nullptr;
  bool value_writable = false;
  if (writable() && info->mutable_dynamic_value != nullptr) {
    value = info->mutable_dynamic_value(const_cast<void*>(storage_));
    value_writable = value != nullptr;
  } else if (info->dynamic_value != nullptr) {
    value = info->dynamic_value(storage_);
  } else {
    return value_ref();
  }

  if (value == nullptr) {
    return {};
  }

  return any_ref::from_raw(get_dynamic_value_type(), value, value_writable);
}

bool indirect_view::can_set_value() const noexcept {
  const meta::indirect_ops* info = data();
  return valid() && info->set_value != nullptr;
}

bool indirect_view::can_set_value_with_context() const noexcept {
  const meta::indirect_ops* info = data();
  return valid() && info->set_value_with_context != nullptr;
}

bool indirect_view::set_value(type_id dynamic_type, void* value) const {
  const meta::indirect_ops* info = data();
  if (!writable() || info->set_value == nullptr || value == nullptr || !dynamic_type) {
    return false;
  }

  void* adjusted = caors::meta::get_registry().upcast(dynamic_type, get_value_type().id(), value);
  if (adjusted == nullptr) {
    return false;
  }

  return info->set_value(const_cast<void*>(storage_), adjusted);
}

bool indirect_view::set_value(type_id dynamic_type, void* value,
                              const traits::wrapper_set_context& context) const {
  const meta::indirect_ops* info = data();
  if (!writable() || info->set_value_with_context == nullptr || value == nullptr || !dynamic_type ||
      !context.valid()) {
    return false;
  }

  void* adjusted = caors::meta::get_registry().upcast(dynamic_type, get_value_type().id(), value);
  if (adjusted == nullptr) {
    return false;
  }

  return info->set_value_with_context(const_cast<void*>(storage_), adjusted, context);
}

bool indirect_view::reset() const {
  const meta::indirect_ops* info = data();
  if (!writable() || info->reset == nullptr) {
    return false;
  }

  return info->reset(const_cast<void*>(storage_));
}

bool indirect_view::ensure_value() const { return ensure_value(nullptr); }

bool indirect_view::ensure_value(memory::memory_resource* resource) const {
  const meta::indirect_ops* info = data();
  if (!writable() || info->ensure_value == nullptr) {
    return false;
  }

  return info->ensure_value(const_cast<void*>(storage_), resource);
}

indirect_view::indirect_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

const meta::indirect_ops* indirect_view::data() const noexcept {
  return type_.get_raw_ops<meta::indirect_ops>();
}

bool pair_view::valid() const noexcept {
  const meta::pair_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

meta::type pair_view::get_first_type() const noexcept {
  const meta::pair_ops* info = data();
  return valid() && info->get_key_type != nullptr ? info->get_key_type() : meta::type{};
}

meta::type pair_view::get_value_type() const noexcept {
  const meta::pair_ops* info = data();
  return valid() && info->get_value_type != nullptr ? info->get_value_type() : meta::type{};
}

any_ref pair_view::first_ref() const {
  const meta::pair_ops* info = data();
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

  return any_ref::from_raw(get_first_type(), value, value_writable);
}

any_ref pair_view::second_ref() const {
  const meta::pair_ops* info = data();
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

  return any_ref::from_raw(get_value_type(), value, value_writable);
}

any_ref pair_view::value_ref() const { return second_ref(); }

bool pair_view::set_first(const any& value) const {
  if (!writable() || !value.valid() || value.get_type().id() != get_first_type().id()) {
    return false;
  }

  any_ref target = first_ref();
  return target.valid() && target.try_set(value);
}

bool pair_view::set_second(const any& value) const {
  if (!writable() || !value.valid() || value.get_type().id() != get_value_type().id()) {
    return false;
  }

  any_ref target = second_ref();
  return target.valid() && target.try_set(value);
}

pair_view::pair_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

const meta::pair_ops* pair_view::data() const noexcept {
  return type_.get_raw_ops<meta::pair_ops>();
}

bool tuple_view::valid() const noexcept {
  const meta::tuple_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

std::size_t tuple_view::size() const noexcept {
  const meta::tuple_ops* info = data();
  return valid() ? info->elements.size() : 0;
}

std::optional<meta::type> tuple_view::get_element_type(std::size_t index) const noexcept {
  const meta::tuple_ops* info = data();
  if (!valid() || index >= info->elements.size()) {
    return std::nullopt;
  }

  const meta::tuple_element_ops& element = info->elements[index];
  meta::type type = element.get_value_type != nullptr ? element.get_value_type() : meta::type{};
  if (!type) {
    return std::nullopt;
  }

  return type;
}

any_ref tuple_view::element_ref(std::size_t index) const {
  const meta::tuple_ops* info = data();
  if (!valid() || index >= info->elements.size()) {
    return {};
  }

  const meta::tuple_element_ops& element_data = info->elements[index];
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

  const meta::type element_type =
      element_data.get_value_type != nullptr ? element_data.get_value_type() : meta::type{};
  return any_ref::from_raw(element_type, value, value_writable);
}

bool tuple_view::set_element(std::size_t index, const any& value) const {
  const std::optional<meta::type> expected = get_element_type(index);
  if (!writable() || !expected.has_value() || !value.valid() ||
      value.get_type().id() != expected->id()) {
    return false;
  }

  any_ref target = element_ref(index);
  return target.valid() && target.try_set(value);
}

tuple_view::tuple_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

const meta::tuple_ops* tuple_view::data() const noexcept {
  return type_.get_raw_ops<meta::tuple_ops>();
}

bool variant_view::valid() const noexcept {
  const meta::variant_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

std::size_t variant_view::size() const noexcept {
  const meta::variant_ops* info = data();
  return valid() ? info->alternatives.size() : 0;
}

std::optional<std::size_t> variant_view::index() const noexcept {
  const meta::variant_ops* info = data();
  if (!valid() || info->index == nullptr) {
    return std::nullopt;
  }

  const std::size_t active = info->index(storage_);
  if (active >= info->alternatives.size()) {
    return std::nullopt;
  }

  return active;
}

std::optional<meta::type>
variant_view::get_alternative_type(std::size_t alternative) const noexcept {
  const meta::variant_ops* info = data();
  if (!valid() || alternative >= info->alternatives.size()) {
    return std::nullopt;
  }

  const meta::variant_alternative_ops& data = info->alternatives[alternative];
  meta::type type = data.get_value_type != nullptr ? data.get_value_type() : meta::type{};
  if (!type) {
    return std::nullopt;
  }

  return type;
}

meta::type variant_view::get_value_type() const noexcept {
  const std::optional<std::size_t> active = index();
  const meta::variant_ops* info = data();
  if (!active.has_value() || info == nullptr || *active >= info->alternatives.size()) {
    return {};
  }

  const meta::variant_alternative_ops& alternative = info->alternatives[*active];
  return alternative.get_value_type != nullptr ? alternative.get_value_type() : meta::type{};
}

any_ref variant_view::value_ref() const {
  const std::optional<std::size_t> active = index();
  if (!active.has_value()) {
    return {};
  }

  const meta::variant_alternative_ops& alternative_data = data()->alternatives[*active];
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

  const meta::type value_type =
      alternative_data.get_value_type != nullptr ? alternative_data.get_value_type() : meta::type{};
  return any_ref::from_raw(value_type, value, value_writable);
}

bool variant_view::emplace(std::size_t alternative) const { return emplace(alternative, nullptr); }

bool variant_view::emplace(std::size_t alternative, memory::memory_resource* resource) const {
  const meta::variant_ops* info = data();
  if (!writable() || alternative >= info->alternatives.size()) {
    return false;
  }

  const std::optional<std::size_t> active = index();
  if (active.has_value() && *active == alternative) {
    return true;
  }

  const meta::variant_alternative_ops& alternative_data = info->alternatives[alternative];
  if (alternative_data.emplace == nullptr) {
    return false;
  }

  return alternative_data.emplace(const_cast<void*>(storage_), resource);
}

bool variant_view::set_value(const any& value) const {
  const type_id expected = get_value_type().id();
  if (!writable() || !expected || !value.valid() || value.get_type().id() != expected) {
    return false;
  }

  any_ref target = value_ref();
  return target.valid() && target.try_set(value);
}

bool variant_view::set_value(std::size_t alternative, const any& value) const {
  const meta::variant_ops* info = data();
  if (!writable() || info == nullptr || alternative >= info->alternatives.size() ||
      !value.valid()) {
    return false;
  }

  const meta::variant_alternative_ops& alternative_data = info->alternatives[alternative];
  const meta::type expected =
      alternative_data.get_value_type != nullptr ? alternative_data.get_value_type() : meta::type{};
  if (!expected || value.get_type().id() != expected.id() || alternative_data.set == nullptr) {
    return false;
  }

  return alternative_data.set(const_cast<void*>(storage_), value.data());
}

variant_view::variant_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

const meta::variant_ops* variant_view::data() const noexcept {
  return type_.get_raw_ops<meta::variant_ops>();
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
  const meta::sequence_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

bool sequence_view::can_read() const noexcept {
  const meta::sequence_ops* info = data();
  return valid() && info->can_read;
}

bool sequence_view::read_in_place() const noexcept {
  const meta::sequence_ops* info = data();
  return valid() && info->read_in_place;
}

std::size_t sequence_view::fixed_size() const noexcept {
  const meta::sequence_ops* info = data();
  return valid() ? info->fixed_size : meta::dynamic_sequence_size;
}

meta::type sequence_view::get_value_type() const noexcept {
  const meta::sequence_ops* info = data();
  return valid() && info->get_value_type != nullptr ? info->get_value_type() : meta::type{};
}

std::optional<std::size_t> sequence_view::size() const {
  const meta::sequence_ops* info = data();
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
  const meta::sequence_ops* info = data();
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

  return any_ref::from_raw(get_value_type(), element, element_writable);
}

bool sequence_view::clear() const {
  const meta::sequence_ops* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool sequence_view::resize(std::size_t size) const { return resize(size, nullptr); }

bool sequence_view::resize(std::size_t size, memory::memory_resource* resource) const {
  const meta::sequence_ops* info = data();
  if (!writable() || info->resize == nullptr) {
    return false;
  }

  return info->resize(const_cast<void*>(storage_), size, resource);
}

bool sequence_view::append_value(const any& value) const {
  const meta::sequence_ops* info = data();
  if (!writable() || info->append == nullptr || !value.valid() ||
      value.get_type().id() != get_value_type().id()) {
    return false;
  }

  return info->append(const_cast<void*>(storage_), value.data());
}

sequence_view::sequence_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

const meta::sequence_ops* sequence_view::data() const noexcept {
  return type_.get_raw_ops<meta::sequence_ops>();
}

set_view::iterator::iterator(const iterator& other) : view_(other.view_) {
  const meta::set_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (other.iterator_ != nullptr && info != nullptr && info->iterator.clone != nullptr) {
    iterator_ = info->iterator.clone(other.iterator_);
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
  const meta::set_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info == nullptr || iterator_ == nullptr || info->iterator_value == nullptr) {
    return {};
  }

  const void* value = info->iterator_value(iterator_);
  const meta::type value_type =
      info->get_value_type != nullptr ? info->get_value_type() : meta::type{};
  return any_ref::from_raw(value_type, value, false);
}

set_view::iterator& set_view::iterator::operator++() noexcept {
  const meta::set_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator.advance != nullptr) {
    info->iterator.advance(iterator_);
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

  const meta::set_ops* info = lhs.view_ != nullptr ? lhs.view_->data() : nullptr;
  if (info == nullptr || info->iterator.equal == nullptr) {
    return false;
  }

  return info->iterator.equal(lhs.iterator_, rhs.iterator_);
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
  const meta::set_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator.destroy != nullptr) {
    info->iterator.destroy(iterator_);
  }

  view_ = nullptr;
  iterator_ = nullptr;
}

bool set_view::valid() const noexcept {
  const meta::set_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

meta::type set_view::get_value_type() const noexcept {
  const meta::set_ops* info = data();
  return valid() && info->get_value_type != nullptr ? info->get_value_type() : meta::type{};
}

std::optional<std::size_t> set_view::size() const {
  const meta::set_ops* info = data();
  if (!valid() || info->size == nullptr) {
    return std::nullopt;
  }

  return info->size(storage_);
}

set_view::iterator set_view::begin() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->iterator.begin(storage_)};
}

set_view::iterator set_view::end() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->iterator.end(storage_)};
}

bool set_view::clear() const {
  const meta::set_ops* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool set_view::insert_value(const any& value) const {
  const meta::set_ops* info = data();
  if (!writable() || info->insert == nullptr || !value.valid() ||
      value.get_type().id() != get_value_type().id()) {
    return false;
  }

  return info->insert(const_cast<void*>(storage_), value.data());
}

set_view::set_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

bool set_view::has_iterator_support() const noexcept {
  const meta::set_ops* info = data();
  return valid() && info->iterator.valid() && info->iterator_value != nullptr;
}

const meta::set_ops* set_view::data() const noexcept { return type_.get_raw_ops<meta::set_ops>(); }

associative_view::iterator::iterator(const iterator& other) : view_(other.view_) {
  const meta::associative_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (other.iterator_ != nullptr && info != nullptr && info->iterator.clone != nullptr) {
    iterator_ = info->iterator.clone(other.iterator_);
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
  const meta::associative_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info == nullptr || iterator_ == nullptr || info->iterator_key == nullptr ||
      info->iterator_value == nullptr) {
    return {};
  }

  const void* key = info->iterator_key(iterator_);
  const void* value = info->iterator_value(iterator_);
  return associative_entry_ref{
      any_ref::from_raw(info->get_key_type != nullptr ? info->get_key_type() : meta::type{}, key,
                        false),
      any_ref::from_raw(info->get_value_type != nullptr ? info->get_value_type() : meta::type{},
                        value, false),
  };
}

associative_view::iterator& associative_view::iterator::operator++() noexcept {
  const meta::associative_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator.advance != nullptr) {
    info->iterator.advance(iterator_);
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

  const meta::associative_ops* info = lhs.view_ != nullptr ? lhs.view_->data() : nullptr;
  if (info == nullptr || info->iterator.equal == nullptr) {
    return false;
  }

  return info->iterator.equal(lhs.iterator_, rhs.iterator_);
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
  const meta::associative_ops* info = view_ != nullptr ? view_->data() : nullptr;
  if (info != nullptr && iterator_ != nullptr && info->iterator.destroy != nullptr) {
    info->iterator.destroy(iterator_);
  }

  view_ = nullptr;
  iterator_ = nullptr;
}

bool associative_view::valid() const noexcept {
  const meta::associative_ops* info = data();
  return info != nullptr && info->valid() && storage_ != nullptr;
}

meta::type associative_view::get_key_type() const noexcept {
  const meta::associative_ops* info = data();
  return valid() && info->get_key_type != nullptr ? info->get_key_type() : meta::type{};
}

meta::type associative_view::get_value_type() const noexcept {
  const meta::associative_ops* info = data();
  return valid() && info->get_value_type != nullptr ? info->get_value_type() : meta::type{};
}

std::optional<std::size_t> associative_view::size() const {
  const meta::associative_ops* info = data();
  if (!valid() || info->size == nullptr) {
    return std::nullopt;
  }

  return info->size(storage_);
}

associative_view::iterator associative_view::begin() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->iterator.begin(storage_)};
}

associative_view::iterator associative_view::end() const {
  if (!has_iterator_support()) {
    return {};
  }

  return iterator{this, data()->iterator.end(storage_)};
}

bool associative_view::clear() const {
  const meta::associative_ops* info = data();
  if (!writable() || info->clear == nullptr) {
    return false;
  }

  info->clear(const_cast<void*>(storage_));
  return true;
}

bool associative_view::insert_value(const any& key, const any& mapped) const {
  const meta::associative_ops* info = data();
  if (!writable() || info->insert == nullptr || !key.valid() || !mapped.valid() ||
      key.get_type().id() != get_key_type().id() ||
      mapped.get_type().id() != get_value_type().id()) {
    return false;
  }

  return info->insert(const_cast<void*>(storage_), key.data(), mapped.data());
}

associative_view::associative_view(meta::type type, const void* storage, bool writable) noexcept
    : type_(type), storage_(storage), writable_(writable) {}

bool associative_view::has_iterator_support() const noexcept {
  const meta::associative_ops* info = data();
  return valid() && info->iterator.valid() && info->iterator_key != nullptr &&
         info->iterator_value != nullptr;
}

const meta::associative_ops* associative_view::data() const noexcept {
  return type_.get_raw_ops<meta::associative_ops>();
}

} // namespace caors::meta
