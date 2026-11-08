cmake_minimum_required(VERSION 3.21)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- BUILD OPTIONS ---

option(DALIA_BUILD_STUDIO "Build the DALIA Studio editor" OFF)
option(DALIA_BUILD_SANDBOX "Build the DALIA Engine sandbox" ${PROJECT_IS_TOP_LEVEL})

if (PROJECT_IS_TOP_LEVEL)
    set(DALIA_PROFILING_DEFAULT ON)
else()
    set(DALIA_PROFILING_DEFAULT ${TRACY_ENABLE})
endif()
option(DALIA_ENABLE_PROFILING "Enable Tracy profiling in DALIA Engine" ${DALIA_PROFILING_DEFAULT})

option(DALIA_BUILD_TESTS "Build the DALIA test suite" ${PROJECT_IS_TOP_LEVEL})

# ------

add_subdirectory(common)
add_subdirectory(engine)

if (DALIA_BUILD_STUDIO)
    add_subdirectory(studio)
endif()

if (DALIA_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if (DALIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
