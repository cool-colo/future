#include <iostream>
#include <thread>
#include <chrono>
#include <numeric>
#include "core.h"
#include "promise.h"
#include "future.h"


int main(){
  std::cout<<"hello world"<<std::endl;
  auto [p, f] = makePromiseContract<int>();


  std::thread t([p = std::move(p)] ()mutable{
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout<<"set result"<<std::endl;
    p.setValue(100);
  });


  auto f1 = std::move(f).then([](int i){
    return i*i;
  }).then([](int i){
    return std::to_string(i);
  }).then([](std::string&& s){
    return std::vector<std::string>{s,s,s,s};
  }).ensure([]{
    std::cout<<"ensure"<<std::endl;
  });


  std::cout<<"waiting for result"<<std::endl;
  for (auto& value : std::move(f1).get()){
    std::cout<<value<<std::endl;
  }
  t.join();

  {
  std::vector<Future<int>> futures;
  for (int i = 1; i <= 10; i++){
    auto [p, f] = makePromiseContract<int>();
    futures.emplace_back(std::move(f));

    std::thread([i, p = std::move(p)] ()mutable{
      std::this_thread::sleep_for(std::chrono::seconds(i));
      p.setValue(int(i));
      std::cout<<"thread setValue:"<<i<<std::endl;
    }).detach();
  }

  auto f2 = collectAll(std::move(futures)).then([](std::vector<int>&& results){
    return std::accumulate(std::begin(results), std::end(results), 0); 
  });
  std::cout<<"start wait collectAll"<<std::endl;
  assert(std::move(f2).get() == 55); 
  }

  {
  std::vector<Future<int>> futures;
  for (int i = 1; i <= 10; i++){
    auto [p, f] = makePromiseContract<int>();
    futures.emplace_back(std::move(f));

    std::thread([i, p = std::move(p)] ()mutable{
      p.setValue(int(i));
    }).detach();
  }

  auto f2 = collectN(std::move(futures),9).then([](std::vector<std::pair<size_t, int>>&& results){
    int sum = 0;
    for(auto[index, value] : results){
      sum += value;
    }
    return sum;
  });
  std::cout<<"start wait collectN"<<std::endl;
  assert(std::move(f2).get() < 55); 
  }

  {
  std::vector<Future<int>> futures;
  for (int i = 1; i <= 10; i++){
    auto [p, f] = makePromiseContract<int>();
    futures.emplace_back(std::move(f));

    std::thread([i, p = std::move(p)] ()mutable{
      p.setValue(int(1));
    }).detach();
  }

  auto f2 = collectAny(std::move(futures));
  std::cout<<"start wait collectAny"<<std::endl;
  assert(std::move(f2).get().second == 1); 
  }
  std::cout<<"finished"<<std::endl;
  return 0;
}
