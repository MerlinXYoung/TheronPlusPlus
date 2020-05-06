cmake_minimum_required(VERSION 3.15)
project(Theron++)


set(CMAKE_CXX_STANDARD_REQUIRED 17)
set(CMAKE_CXX_STANDARD 17)

file(GLOB SRCS "Utility/*.cpp" "Communication/*.cpp" "Communication/AMQ/*.cpp")
# file(GLOB COMMUN_SRC "Communication/*.cpp")

find_package(Threads REQUIRED)
# find_package(Arp1 REQUIRED)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(/usr/include/apr-1.0)
include_directories(/usr/local/activemq-cpp/include/activemq-cpp-3.9.5)
include_directories(/usr/local/libzmq/include)
include_directories(/usr/local/cppzmq/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(Theron++ ${SRCS} Actor.cpp )
# target_link_libraries(Theron++ )

add_subdirectory(Examples)