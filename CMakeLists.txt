cmake_minimum_required(VERSION 2.4)



project (future)


set(CMAKE_CXX_FLAGS "-DUSESSE -DSSEOPT -DUSEOMP -g -fopenmp -Werror=return-type -Wno-deprecated -Wno-register --std=c++17 ${CMAKE_CXX_FLAGS} " )

file(GLOB SRC *.cpp)
add_executable(future ${SRC})
