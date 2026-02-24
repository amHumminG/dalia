cmake_minimum_required(VERSION 3.20)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(engine)
add_subdirectory(studio)
add_subdirectory(common)
add_subdirectory(demo)