#include "core.h"


bool CoreBase::hasCallback() const noexcept {
  constexpr auto allowed = State::OnlyCallback | State::Done | State::Empty;
  auto const state = state_.load(std::memory_order_acquire);
  return State() != (state & allowed);
} 



bool CoreBase::hasResult() const noexcept {
  constexpr auto allowed = State::OnlyResult | State::Done;
  auto core = this;
  auto state = core->state_.load(std::memory_order_acquire);
  return State() != (state & allowed);
}

bool CoreBase::ready() const noexcept {
  return hasResult();
}


void CoreBase::setResult_() {
  assert(!hasResult());

  auto state = state_.load(std::memory_order_acquire);
  switch (state) {
    case State::Start:
      if (state_.compare_exchange_strong(state, State::OnlyResult, std::memory_order_release, std::memory_order_acquire)){
        return;
      }
    case State::OnlyCallback:
      state_.store(State::Done, std::memory_order_relaxed);
      doCallback(state);
      return;
    case State::OnlyResult:
    case State::Done:
    case State::Empty:
    default:
      throw std::logic_error("setResult unexpected state");
  }
}


void CoreBase::setCallback_(Callback&& callback) {
  assert(!hasCallback());

  ::new (&callback_) Callback(std::move(callback));

  auto state = state_.load(std::memory_order_acquire);
  State nextState = State::OnlyCallback;

  if (state == State::Start) {
    if (state_.compare_exchange_strong(state, nextState, std::memory_order_release, std::memory_order_acquire)){
      return;
    }
  }

  if (state == State::OnlyResult) {
    state_.store(State::Done, std::memory_order_relaxed);
    doCallback(state);
    return;
  }

  throw std::logic_error("setCallback unexpected state");
}


void CoreBase::doCallback(State priorState) {
  assert(state_ == State::Done);
  callback_(*this);
  callback_.~Callback();
}

