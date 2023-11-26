cmake_minimum_required(VERSION 3.18)
project(u1)

set(CMAKE_CXX_STANDARD 11)
find_package(Threads REQUIRED)

add_executable(u1 main.cpp)

target_link_libraries(u1 ${CMAKE_THREAD_LIBS_INIT} )