cmake_minimum_required(VERSION 3.24)
project(HunghereseWeb)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost COMPONENTS system filesystem REQUIRED)

add_executable(HunghereseWeb main.cpp)

target_include_directories(HunghereseWeb PRIVATE /usr/include/gx/)
target_link_libraries(HunghereseWeb Boost::system Boost::filesystem)
