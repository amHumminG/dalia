cmake_minimum_required(VERSION 3.20)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DALIA_BUILD_STUDIO "Build the DALIA Studio editor" ON)
option(DALIA_BUILD_DEMO "Build the DALIA Engine demo" ON)

add_subdirectory(engine)

if (DALIA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

add_subdirectory(common)

if (DALIA_BUILD_DEMO)
    add_subdirectory(demo)
endif()