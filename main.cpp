#include <iostream>
#include <thread>
#include <chrono>
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


  auto f1 = std::move(f).thenValue([](int i){
    return i*i;
  }).thenValue([](int i){
    return std::to_string(i);
  }).thenValue([](std::string&& s){
    return std::vector<std::string>{s,s,s,s};
  });


  std::cout<<"waiting for result"<<std::endl;
  for (auto& value : std::move(f1).get()){
    std::cout<<value<<std::endl;
  }

  t.join();
  return 0;
}
