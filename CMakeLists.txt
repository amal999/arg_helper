cmake_minimum_required(VERSION 3.10)
project(arg_helper)

set(CMAKE_CXX_STANDARD 17)
find_package(nlohmann_json REQUIRED) # system package or installed via package manager

add_executable(arg_helper main.cpp)
target_link_libraries(arg_helper PRIVATE nlohmann_json::nlohmann_json)
