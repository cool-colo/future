#pragma once
#include <memory>
#include "semaphore.h"


namespace detail {



template <class FutureType, typename T = typename FutureType::value_type>
void waitImpl(FutureType& f) {
  // short-circuit if there's nothing to do
  if (f.isReady()) {
    return;
  }

  auto p = std::make_shared<Promise<T>>();
  auto r = p->getFuture();

  Semaphore semaphore;
  f.setCallback_([&semaphore, p](T&& t) mutable {
    p->setValue(std::move(t));
    semaphore.notify();
  });
  f = std::move(r);
  semaphore.wait();
  assert(f.isReady());
}




}

template <class T>
Future<T> makeFuture(T&& t) {
  return Future<T>(std::shared_ptr<Core<T>>(Core<T>::make(std::move(t))));
}

template <class T>
FutureBase<T>::FutureBase(Future<T>&& other) noexcept : core_(other.core_) {
  other.core_ = nullptr;
}


template <class T>
void FutureBase<T>::assign(FutureBase<T>&& other) noexcept {
  core_ = std::exchange(other.core_, nullptr);
}

template <class T>
FutureBase<T>::~FutureBase() {
}

template <class T>
T& FutureBase<T>::value() & {
  return getCoreValueChecked();
}

template <class T>
T const& FutureBase<T>::value() const& {
  return getCoreValueChecked();
}

template <class T>
T&& FutureBase<T>::value() && {
  return std::move(getCoreValueChecked());
}

template <class T>
T const&& FutureBase<T>::value() const&& {
  return std::move(getCoreValueChecked());
}


template <class T>
bool FutureBase<T>::isReady() const {
  return getCore().hasResult();
}

template <class T>
void FutureBase<T>::throwIfInvalid() const {
  if (!core_) {
    throw FutureInvalid();
  }
}

template <class T>
void FutureBase<T>::throwIfContinued() const {
  if (!core_ || core_->hasCallback()) {
    throw FutureAlreadyContinued();
  }
}

template <class T>
std::optional<T> FutureBase<T>::poll() {
  auto& core = getCore();
  return core.hasResult() ? std::optional<T>(std::move(core.get()))
                          : std::optional<T>();
}


template <class T>
template <class F>
void FutureBase<T>::setCallback_(F&& func) {
  throwIfContinued();
  getCore().setCallback(static_cast<F&&>(func));
}


// Variant: returns a value
// e.g. f.then([](Try<T>&& t){ return t.value(); });
template <class T>
template <typename F, typename R>
typename std::enable_if<!R::ReturnsFuture::value, typename R::Return>::type
FutureBase<T>::thenImplementation(
    F&& func, R) {
  static_assert(R::Arg::ArgsSize::value == 1, "Then must take one arguments");
  typedef typename R::ReturnsFuture::Inner B;

  auto p = std::make_shared<Promise<B>>();
  auto f = p->getFuture();

  this->setCallback_([p, func = static_cast<F&&>(func)](T&& t) mutable {
    p->setValue(std::move(static_cast<F&&>(func)(std::move(t))));
  });

  return std::move(f);
}

// Variant: returns a Future
// e.g. f.then([](T&& t){ return makeFuture<T>(t); });
template <class T>
template <typename F, typename R>
typename std::enable_if<R::ReturnsFuture::value, typename R::Return>::type
FutureBase<T>::thenImplementation(
    F&& func, R) {
  static_assert(R::Arg::ArgsSize::value == 1, "Then must take one arguments");
  typedef typename R::ReturnsFuture::Inner B;


  auto p = std::make_shared<Promise<B>>();
  auto f = p->getFuture();

  this->setCallback_([p, func = static_cast<F&&>(func)](T&& t) mutable {
    p->setValue(std::move(static_cast<F&&>(func)(std::move(t))).value());
  });

  return f;
}



template <class T>
Future<T>::Future(Future<T>&& other) noexcept
    : FutureBase<T>(std::move(other)) {}

template <class T>
Future<T>& Future<T>::operator=(Future<T>&& other) noexcept {
  this->assign(std::move(other));
  return *this;
}

// unwrap

template <class T>
template <class F>
typename std::
    enable_if<isFuture<F>::value, Future<typename isFuture<T>::Inner>>::type
    Future<T>::unwrap() && {
  return std::move(*this).then(
      [](Future<typename isFuture<T>::Inner> internal_future) {
        return internal_future;
      });
}

template <class T>
template <typename F>
Future<typename valueCallableResult<T, F>::value_type>
Future<T>::then(F&& func) && {
  auto lambdaFunc = [f = static_cast<F&&>(func)](
                        T&& t) mutable {
    return static_cast<F&&>(f)(std::move(t));
  };
  using R = valueCallableResult<T, decltype(lambdaFunc)>;
  return this->thenImplementation(
      std::move(lambdaFunc), R{});
}


template <class T>
template <class F>
Future<T> Future<T>::ensure(F&& func) && {
  return std::move(*this).then(
      [funcw = static_cast<F&&>(func)](T&& t) mutable {
        static_cast<F&&>(funcw)();
        return makeFuture(std::move(t));
      });
}


template <class T>
Future<T>& Future<T>::wait() & {
  detail::waitImpl(*this);
  return *this;
}

template <class T>
Future<T>&& Future<T>::wait() && {
  detail::waitImpl(*this);
  return std::move(*this);
}

template <class T>
T Future<T>::get() && {
  wait();
  return std::move(std::move(*this).value());
}


template <class T>
Future<std::vector<T>> collectAll(std::vector<Future<T>>&& futures){
  return collectAll(futures.begin(), futures.end());
}


template <class InputIterator>
Future<std::vector<
    typename std::iterator_traits<InputIterator>::value_type::value_type>>
collectAll(InputIterator first, InputIterator last) {
  using F = typename std::iterator_traits<InputIterator>::value_type;
  using T = typename F::value_type;

  struct Context {
    explicit Context(size_t n) : results(n) {}
    ~Context() {
      p.setValue(std::move(std::move(results)));
    }
    Promise<std::vector<T>> p;
    std::vector<T> results;
  };


  auto ctx = std::make_shared<Context>(size_t(std::distance(first, last)));

  for (size_t i = 0; first != last; ++first, ++i) {
    first->setCallback_(
        [i, ctx](T&& t) {
          ctx->results[i] = std::move(t);
        });
  }

  return ctx->p.getFuture();
}

template <class T>
Future<std::vector<std::pair<size_t, T>>> collectN(std::vector<Future<T>>&& futures, size_t n){
  return collectN(futures.begin(), futures.end(), n);
}


// collectN (iterator)

template <class InputIterator>
Future<std::vector<std::pair<
    size_t,
    typename std::iterator_traits<InputIterator>::value_type::value_type>>>
collectN(InputIterator first, InputIterator last, size_t n) {
  using F = typename std::iterator_traits<InputIterator>::value_type;
  using T = typename F::value_type;
  using Result = std::vector<std::pair<size_t, T>>;

  struct Context {
    explicit Context(size_t numFutures, size_t min_)
        : v(numFutures), min(min_) {}

    std::vector<std::optional<T>> v;
    size_t min;
    std::atomic<size_t> completed = {0}; // # input futures completed
    std::atomic<size_t> stored = {0}; // # output values stored
    Promise<Result> p;
  };

  assert(n > 0);
  assert(std::distance(first, last) >= 0);

  auto ctx = std::make_shared<Context>(size_t(std::distance(first, last)), n);
  for (size_t i = 0; first != last; ++first, ++i) {
    first->setCallback_([i, ctx](T&& t) {
      // relaxed because this guards control but does not guard data
      auto const c = 1 + ctx->completed.fetch_add(1, std::memory_order_relaxed);
      if (c > ctx->min) {
        return;
      }
      ctx->v[i] = std::move(t);

      // release because the stored values in all threads must be visible below
      // acquire because no stored value is permitted to be fetched early
      auto const s = 1 + ctx->stored.fetch_add(1, std::memory_order_acq_rel);
      if (s < ctx->min) {
        return;
      }
      Result result;
      result.reserve(ctx->completed.load());
      for (size_t j = 0; j < ctx->v.size(); ++j) {
        auto& entry = ctx->v[j];
        if (entry.has_value()) {
          result.emplace_back(j, std::move(entry).value());
        }
      }
      ctx->p.setValue(std::move(result));
    });
  }

  return ctx->p.getFuture();
}


template <class T>
Future<std::pair<size_t, T>> collectAny(std::vector<Future<T>>&& futures){
  return collectAny(futures.begin(), futures.end());
}


// collectAny (iterator)

template <class InputIterator>
Future<std::pair<
    size_t,
    typename std::iterator_traits<InputIterator>::value_type::value_type>>
collectAny(InputIterator first, InputIterator last) {
  using F = typename std::iterator_traits<InputIterator>::value_type;
  using T = typename F::value_type;

  struct Context {
    Promise<std::pair<size_t, T>> p;
    std::atomic<bool> done{false};
  };

  auto ctx = std::make_shared<Context>();
  for (size_t i = 0; first != last; ++first, ++i) {
    first->setCallback_([i, ctx](T&& t) {
      if (!ctx->done.exchange(true, std::memory_order_relaxed)) {
        ctx->p.setValue(std::make_pair(i, std::move(t)));
      }
    });
  }
  return ctx->p.getFuture();
}
