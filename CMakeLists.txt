cmake_minimum_required(VERSION 3.20)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check if Dalia is the main project or included as a submodule
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(DALIA_IS_TOP_LEVEL ON)
else()
    set(DALIA_IS_TOP_LEVEL OFF)
endif()

option(DALIA_BUILD_STUDIO "Build the DALIA Studio editor" ${DALIA_IS_TOP_LEVEL})
option(DALIA_BUILD_DEMO "Build the DALIA Engine demo" ${DALIA_IS_TOP_LEVEL})

add_subdirectory(common)
add_subdirectory(engine)

if (DALIA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

if (DALIA_BUILD_DEMO)
    add_subdirectory(demo)
endif()