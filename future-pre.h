#pragma once
#include <type_traits>


template <std::size_t I>
using index_constant = std::integral_constant<std::size_t, I>;

template <class>
class Promise;


template <class>
class Future;

template <typename T>
struct isFuture : std::false_type {
  using Inner = T;
};

template <typename T>
struct isFuture<Future<T>> : std::true_type {
  typedef T Inner;
};


template <typename...>
struct ArgType;

template <typename Arg, typename... Args>
struct ArgType<Arg, Args...> {
  typedef Arg FirstArg;
  typedef ArgType<Args...> Tail;
};

template <>
struct ArgType<> {
  typedef void FirstArg;
};

template <typename F, typename... Args>
struct argResult {
  using Function = F;
  using ArgList = ArgType<Args...>;
  using Result = std::invoke_result_t<F, Args...>;
  using ArgsSize = index_constant<sizeof...(Args)>;
};

template <typename T, typename F>
struct valueCallableResult {
  typedef argResult<F, T&&> Arg;
  typedef isFuture<typename Arg::Result> ReturnsFuture;
  typedef typename ReturnsFuture::Inner value_type;
  typedef typename Arg::ArgList::FirstArg FirstArg;
  typedef Future<value_type> Return;
};
