#pragma once
#include <functional>
#include <cassert>
#include <atomic>
#include <utility>


enum class State : uint8_t {
  Start = 1 << 0,
  OnlyResult = 1 << 1,
  OnlyCallback = 1 << 2,
  Done = 1 << 3,
  Empty = 1 << 4,
};
constexpr State operator&(State a, State b) {
  return State(uint8_t(a) & uint8_t(b));
}
constexpr State operator|(State a, State b) {
  return State(uint8_t(a) | uint8_t(b));
}
constexpr State operator^(State a, State b) {
  return State(uint8_t(a) ^ uint8_t(b));
}
constexpr State operator~(State a) {
  return State(~uint8_t(a));
}


class CoreBase {
public:
  using Callback = std::function<void(CoreBase&)>;
  CoreBase(const CoreBase&) = delete;
  CoreBase& operator=(const CoreBase&) = delete;
  CoreBase(CoreBase&&) = delete;
  CoreBase& operator=(CoreBase&&) = delete;

  CoreBase(State state): state_(state){}
  virtual ~CoreBase(){}

  void setResult_();
  void setCallback_(Callback&& callback);
  bool hasCallback() const noexcept;
  bool hasResult() const noexcept;
  bool ready() const noexcept;
  void doCallback(State priorState);



  union {
    Callback callback_;
  };
  std::atomic<State> state_;

};

template <typename T>
class ResultHolder{
protected:
  ResultHolder(){}
  ~ResultHolder(){}
  
  union {
    T result_;
  };
};


template <typename T>
class Core :private ResultHolder<T>, public CoreBase {
public:
  static_assert(!std::is_void<T>::value, "void futures are not supported, Use Unit instead.");
  using Result = T; 
  static Core* make() { return new Core(); }
  static Core* make(T&& t) { return new Core(std::move(t)); }
  template<typename ... Args>
  static Core* make(std::in_place_t, Args&&... args){
    return new Core(std::in_place, std::forward<Args&&>(args)...);
  }

  T& get() {
    assert(hasResult());
    return this->result_;
  }
  const T& get() const {
    static_assert(hasResult());
    return this->result_;
  }

  template<typename F>
  void setCallback(F&& func){
    Callback callback = [func = static_cast<F&&>(func)](CoreBase& coreBase) mutable {
      auto& core = static_cast<Core&>(coreBase);
      func(std::move(core.result_));
    };
    setCallback_(std::move(callback));
  }

  void setResult(T&& t){
    new (&this->result_)Result(std::move(t));
    setResult_();
  }

  Core() : CoreBase(State::Start){}
  explicit Core(T&& t) : CoreBase(State::OnlyResult){
    new (&this->result_) Result(std::move(t));
  }
  template<typename ... Args>
  explicit Core(std::in_place_t, Args&& ... args) : CoreBase(State::OnlyResult){
    new (&this->result_) Result(std::in_place, std::forward<Args&&>(args)...);
  }
  ~Core() override {
    auto state = state_.load(std::memory_order_relaxed);
    switch (state) {
      case State::OnlyResult:
        [[fallthrought]]

      case State::Done:
        this->result_.~Result();
        break;

      case State::Empty:
        break;

      case State::Start:
      case State::OnlyCallback:
      default:
        throw std::logic_error("~Core unexpected state");
    }
  }
  
};
