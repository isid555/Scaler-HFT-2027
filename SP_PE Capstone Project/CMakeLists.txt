cmake_minimum_required(VERSION 3.10)
project(HFT_System)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")

find_package(fmt REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_executable(publisher publisher.cpp)
target_link_libraries(publisher fmt::fmt Boost::system rt pthread)

add_executable(consumer_shm consumer_shm.cpp)
target_link_libraries(consumer_shm fmt::fmt rt pthread)

add_executable(consumer_tcp consumer_tcp.cpp)
target_link_libraries(consumer_tcp fmt::fmt Boost::system pthread)

