#pragma once
#include <memory>


class PromiseException : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

class PromiseInvalid : public PromiseException {
 public:
  PromiseInvalid() : PromiseException("Promise invalid") {}
};

class PromiseAlreadySatisfied : public PromiseException {
 public:
  PromiseAlreadySatisfied() : PromiseException("Promise already satisfied") {}
};

class FutureAlreadyRetrieved : public PromiseException {
 public:
  FutureAlreadyRetrieved() : PromiseException("Future already retrieved") {}
};

class BrokenPromise : public PromiseException {
 public:
  explicit BrokenPromise(const std::string& type)
      : PromiseException("Broken promise for type name `" + type + '`') {}

  explicit BrokenPromise(const char* type) : BrokenPromise(std::string(type)) {}
};


template <class T>
class Future;

template <class T>
class Promise;

namespace detail {
  template <class T>
  class FutureBase;

  struct EmptyConstruct {};

  template <typename T, typename F>
  class CoreCallbackState;
}


template <typename T>
class Promise {
public:
  Promise();
  ~Promise();

  Promise(const Promise&) = delete;
  Promise& operator=(const Promise&) = delete;

  Promise(Promise&&) noexcept;
  Promise& operator=(Promise&&) noexcept;


  Future<T> getFuture();

  template <class F>
  void setWith(F&& func);
  void setValue(T&& t);

  
  
private:
  bool retrieved_;
  std::shared_ptr<Core<T>> core_;


  bool isFulfilled() const noexcept;
  void throwIfFulfilled() const;


  std::shared_ptr<Core<T>> getSharedCore() {return core_;}

  Core<T>& getCore() { return getCoreImpl(core_); }
  const Core<T>& getCore() const { return getCoreImpl(core_); }

  template <typename SharedCoreT>
  static Core<T>& getCoreImpl(SharedCoreT& core) {
    if (!core) {
      throw PromiseInvalid();
    }
    return *core;
  }

};

#include "promise-inl.h"
