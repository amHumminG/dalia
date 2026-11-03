cmake_minimum_required(VERSION 3.20)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- BUILD OPTIONS ---

# Check if Dalia is the main project (CMake only)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(DALIA_IS_TOP_LEVEL ON)
else()
    set(DALIA_IS_TOP_LEVEL OFF)
endif()

option(DALIA_BUILD_STUDIO "Build the DALIA Studio editor" OFF)
option(DALIA_BUILD_SANDBOX "Build the DALIA Engine sandbox" ${DALIA_IS_TOP_LEVEL})

if (DALIA_IS_TOP_LEVEL)
    set(DALIA_PROFILING_DEFAULT ON)
else()
    set(DALIA_PROFILING_DEFAULT ${TRACY_ENABLE})
endif()
option(DALIA_ENABLE_PROFILING "Enable Tracy profiling in DALIA Engine" ${DALIA_PROFILING_DEFAULT})

# ------

add_subdirectory(common)
add_subdirectory(engine)

if (DALIA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

if (DALIA_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()