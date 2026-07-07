// SPDX-License-Identifier: MIT
// Copyright (c) 2026 CAORS contributors

#ifndef CAORS_EVENTS_DELEGATE_H_
#define CAORS_EVENTS_DELEGATE_H_

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>

namespace caors::events {

template <typename Signature> class delegate;

/**
 * @brief 轻量非拥有函数委托。
 *
 * `delegate<R(Args...)>` 保存一个调用入口和一个非拥有对象指针，不分配内存，也不接管 callable
 * 生命周期。绑定成员函数或函数对象时，目标对象必须比 delegate 以及保存它的 signal 活得更久。
 */
template <typename Result, typename... Args> class delegate<Result(Args...)> final {
public:
  using result_type = Result;
  using invoke_fn = Result (*)(const void* payload, Args... args);

  constexpr delegate() noexcept = default;

  /** @brief 判断当前是否已经绑定可调用对象。 */
  [[nodiscard]] explicit constexpr operator bool() const noexcept { return invoke_ != nullptr; }

  /** @brief 清空当前绑定。 */
  void reset() noexcept {
    payload_ = nullptr;
    invoke_ = nullptr;
  }

  /** @brief 绑定编译期已知的自由函数、静态函数或无捕获 callable。 */
  template <auto Candidate> void connect() noexcept {
    static_assert(std::is_invocable_r_v<Result, decltype(Candidate), Args...>,
                  "Candidate is not compatible with delegate signature");
    payload_ = nullptr;
    invoke_ = [](const void*, Args... args) -> Result {
      if constexpr (std::is_void_v<Result>) {
        (void)std::invoke(Candidate, std::forward<Args>(args)...);
      } else {
        return std::invoke(Candidate, std::forward<Args>(args)...);
      }
    };
  }

  /** @brief 绑定成员函数。对象由调用方持有，delegate 不负责生命周期。 */
  template <auto Member, typename Type> void connect(Type* instance) noexcept {
    static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                  "Member must be a member function pointer");
    static_assert(std::is_invocable_r_v<Result, decltype(Member), Type&, Args...>,
                  "Member is not compatible with delegate signature");
    payload_ = instance;
    invoke_ = [](const void* payload, Args... args) -> Result {
      Type* object = static_cast<Type*>(const_cast<void*>(payload));
      if constexpr (std::is_void_v<Result>) {
        (void)std::invoke(Member, *object, std::forward<Args>(args)...);
      } else {
        return std::invoke(Member, *object, std::forward<Args>(args)...);
      }
    };
  }

  /** @brief 绑定 const 成员函数。对象由调用方持有，delegate 不负责生命周期。 */
  template <auto Member, typename Type> void connect(const Type* instance) noexcept {
    static_assert(std::is_member_function_pointer_v<decltype(Member)>,
                  "Member must be a member function pointer");
    static_assert(std::is_invocable_r_v<Result, decltype(Member), const Type&, Args...>,
                  "Member is not compatible with delegate signature");
    payload_ = instance;
    invoke_ = [](const void* payload, Args... args) -> Result {
      const Type* object = static_cast<const Type*>(payload);
      if constexpr (std::is_void_v<Result>) {
        (void)std::invoke(Member, *object, std::forward<Args>(args)...);
      } else {
        return std::invoke(Member, *object, std::forward<Args>(args)...);
      }
    };
  }

  /** @brief 绑定函数对象左值。函数对象由调用方持有，delegate 不复制也不延长生命周期。 */
  template <typename Callable> void connect(Callable& callable) noexcept {
    static_assert(std::is_invocable_r_v<Result, Callable&, Args...>,
                  "Callable is not compatible with delegate signature");
    payload_ = &callable;
    invoke_ = [](const void* payload, Args... args) -> Result {
      Callable* callable = static_cast<Callable*>(const_cast<void*>(payload));
      if constexpr (std::is_void_v<Result>) {
        (void)std::invoke(*callable, std::forward<Args>(args)...);
      } else {
        return std::invoke(*callable, std::forward<Args>(args)...);
      }
    };
  }

  /** @brief 绑定 const 函数对象左值。函数对象由调用方持有，delegate 不复制也不延长生命周期。 */
  template <typename Callable> void connect(const Callable& callable) noexcept {
    static_assert(std::is_invocable_r_v<Result, const Callable&, Args...>,
                  "Callable is not compatible with delegate signature");
    payload_ = &callable;
    invoke_ = [](const void* payload, Args... args) -> Result {
      const Callable* callable = static_cast<const Callable*>(payload);
      if constexpr (std::is_void_v<Result>) {
        (void)std::invoke(*callable, std::forward<Args>(args)...);
      } else {
        return std::invoke(*callable, std::forward<Args>(args)...);
      }
    };
  }

  /** @brief 调用已绑定目标。未绑定时属于调用方错误，调试构建会触发断言。 */
  Result operator()(Args... args) const {
    assert(invoke_ != nullptr);
    if constexpr (std::is_void_v<Result>) {
      invoke_(payload_, std::forward<Args>(args)...);
    } else {
      return invoke_(payload_, std::forward<Args>(args)...);
    }
  }

private:
  const void* payload_ = nullptr;
  invoke_fn invoke_ = nullptr;
};

} // namespace caors::events

namespace caors {

template <typename Signature> using delegate = events::delegate<Signature>;

} // namespace caors

#endif
