#pragma once
#include<cassert>
#include "future-pre.h"


class FutureException : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

class FutureInvalid : public FutureException {
 public:
  FutureInvalid() : FutureException("Future invalid") {}
};

class FutureAlreadyContinued : public FutureException {
 public:
  FutureAlreadyContinued() : FutureException("Future already continued") {}
};

class FutureNotReady : public FutureException {
 public:
  FutureNotReady() : FutureException("Future not ready") {}
};

class FutureCancellation : public FutureException {
 public:
  FutureCancellation() : FutureException("Future was cancelled") {}
};

class FutureTimeout : public FutureException {
 public:
  FutureTimeout() : FutureException("Timed out") {}
};

class FuturePredicateDoesNotObtain : public FutureException {
 public:
  FuturePredicateDoesNotObtain()
      : FutureException("Predicate does not obtain") {}
};

class FutureNoTimekeeper : public FutureException {
 public:
  FutureNoTimekeeper() : FutureException("No timekeeper available") {}
};

class FutureNoExecutor : public FutureException {
 public:
  FutureNoExecutor() : FutureException("No executor provided to via") {}
};


template <class T>
class Future;

template<typename T>
class FutureBase {
public:
  using value_type = T;

  FutureBase(FutureBase<T> const&) = delete;
  FutureBase(Future<T>&&) noexcept;

  // not copyable
  FutureBase(Future<T> const&) = delete;

  virtual ~FutureBase();


  T& value() &;
  T const& value() const&;
  T&& value() &&;
  T const&& value() const&&;

  bool isReady() const;
  bool hasValue() const;
  std::optional<T> poll();

  template <class F>
  void setCallback_(F&& func);




protected:
  friend class Promise<T>;

  template <class>
  friend class Future;

  Core<T>& getCore() { return getCoreImpl(*this); }
  Core<T> const& getCore() const { return getCoreImpl(*this); }

  template <typename Self>
  static decltype(auto) getCoreImpl(Self& self) {
    if (!self.core_) {
      throw FutureInvalid();
    }
    return *self.core_;
  }

  T& getCoreValueChecked() { return getCoreValueChecked(*this); }
  T const& getCoreValueChecked() const { return getCoreValueChecked(*this); }

  template <typename Self>
  static decltype(auto) getCoreValueChecked(Self& self) {
    auto& core = self.getCore();
    if (!core.hasResult()) {
      throw FutureNotReady();
    }
    return core.get();
  }

  std::shared_ptr<Core<T>> core_;

  explicit FutureBase(std::shared_ptr<Core<T>> obj) : core_(obj) {}



  void throwIfInvalid() const;
  void throwIfContinued() const;

  void assign(FutureBase<T>&& other) noexcept;

  // Variant: returns a value
  // e.g. f.thenTry([](Try<T> t){ return t.value(); });
  template <typename F, typename R>
  typename std::enable_if<!R::ReturnsFuture::value, typename R::Return>::type
  thenImplementation(F&& func, R);

  // Variant: returns a Future
  // e.g. f.thenTry([](Try<T> t){ return makeFuture<T>(t); });
  template <typename F, typename R>
  typename std::enable_if<R::ReturnsFuture::value, typename R::Return>::type
  thenImplementation(F&& func, R);

};


template <class T>
class Future : private FutureBase<T> {
 private:
  using Base = FutureBase<T>;

 public:
  /// Type of the value that the producer, when successful, produces.
  using typename Base::value_type;

  Future(Future<T> const&) = delete;
  // movable
  Future(Future<T>&&) noexcept;


  using Base::isReady;
  using Base::poll;
  using Base::setCallback_;
  using Base::value;

  /// Creates/returns an invalid Future, that is, one with no shared state.
  ///
  /// Postcondition:
  ///
  /// - `RESULT.valid() == false`
  static Future<T> makeEmpty();

  // not copyable
  Future& operator=(Future const&) = delete;

  // movable
  Future& operator=(Future&&) noexcept;


  /// Unwraps the case of a Future<Future<T>> instance, and returns a simple
  /// Future<T> instance.
  ///
  /// Preconditions:
  ///
  /// - `valid() == true` (else throws FutureInvalid)
  ///
  /// Postconditions:
  ///
  /// - Calling code should act as if `valid() == false`,
  ///   i.e., as if `*this` was moved into RESULT.
  /// - `RESULT.valid() == true`
  template <class F = T>
  typename std::
      enable_if<isFuture<F>::value, Future<typename isFuture<T>::Inner>>::type
      unwrap() &&;




  /// When this Future has completed, execute func which is a function that
  /// can be called with `T&&` (often a lambda with parameter type
  /// `auto&&` or `auto`).
  ///
  /// Func shall return either another Future or a value.
  ///
  /// Versions of these functions with Inline in the name will run the
  /// continuation inline with the execution of the previous callback in the
  /// chain if the callback attached to the previous future that triggers
  /// execution of func runs on the same executor that func would be executed
  /// on.
  ///
  /// A Future for the return type of func is returned.
  ///
  ///   Future<string> f2 = f1.thenValue([](auto&& v) {
  ///     ...
  ///     return string("foo");
  ///   });
  ///
  /// Preconditions:
  ///
  /// - `valid() == true` (else throws FutureInvalid)
  ///
  /// Postconditions:
  ///
  /// - `valid() == false`
  /// - `RESULT.valid() == true`
  template <typename F>
  Future<typename valueCallableResult<T, F>::value_type>
  thenValue(F&& func) &&;







  /// func is like std::function<void()> and is executed unconditionally, and
  /// the value/exception is passed through to the resulting Future.
  /// func shouldn't throw, but if it does it will be captured and propagated,
  /// and discard any value/exception that this Future has obtained.
  ///
  /// Preconditions:
  ///
  /// - `valid() == true` (else throws FutureInvalid)
  ///
  /// Postconditions:
  ///
  /// - Calling code should act as if `valid() == false`,
  ///   i.e., as if `*this` was moved into RESULT.
  /// - `RESULT.valid() == true`
  template <class F>
  Future<T> ensure(F&& func) &&;


  T get() &&;

  Future<T>& wait() &;

  Future<T>&& wait() &&;

 protected:
  friend class Promise<T>;
  template <class>
  friend class FutureBase;

  template <class>
  friend class Future;

  template <class>
  friend class FutureSplitter;

  using Base::throwIfContinued;
  using Base::throwIfInvalid;

  explicit Future(std::shared_ptr<Core<T>> obj) : Base(obj) {}


};

template <class T>
std::pair<Promise<T>, Future<T>> makePromiseContract() {
  auto p = Promise<T>();
  auto f = p.getFuture();
  return std::make_pair(std::move(p), std::move(f));
}

#include "future-inl.h"
