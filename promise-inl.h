#pragma once




template <class T>
Promise<T>::Promise() : retrieved_(false), core_(Core<T>::make()) {}


template <class T>
Promise<T>::Promise(Promise<T>&& other) noexcept
    : retrieved_(std::exchange(other.retrieved_, false)),
      core_(std::exchange(other.core_, nullptr)) {}


template <class T>
Promise<T>& Promise<T>::operator=(Promise<T>&& other) noexcept {
  retrieved_ = std::exchange(other.retrieved_, false);
  core_ = std::exchange(other.core_, nullptr);
  return *this;
}

template <class T>
void Promise<T>::throwIfFulfilled() const {
  if (getCore().hasResult()) {
    throw PromiseAlreadySatisfied();
  }
}


template <class T>
Promise<T>::~Promise() {
}


template <class T>
Future<T> Promise<T>::getFuture() {
  if (retrieved_) {
    throw FutureAlreadyRetrieved();
  }
  retrieved_ = true;
  return Future<T>(getSharedCore());
}



template <class T>
void Promise<T>::setValue(T&& t) {
  throwIfFulfilled();
  getCore().setResult(std::move(t));
}


template <class T>
template <class F>
void Promise<T>::setWith(F&& func) {
  throwIfFulfilled();
  setValue(std::move(func()));
}


template <class T>
bool Promise<T>::isFulfilled() const noexcept {
  return getCore().hasResult();
}


