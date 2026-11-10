cmake_minimum_required(VERSION 3.21)
project(dalia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- BUILD OPTIONS ---

option(DALIA_BUILD_STUDIO "Build the Dalia Studio editor" OFF)
message(STATUS "Dalia - DALIA_BUILD_STUDIO=${DALIA_BUILD_STUDIO}")

option(DALIA_BUILD_SANDBOX "Build the Dalia Engine sandbox" ${PROJECT_IS_TOP_LEVEL})
message(STATUS "Dalia - DALIA_BUILD_SANDBOX=${DALIA_BUILD_SANDBOX}")

option(DALIA_BUILD_TESTS "Build the Dalia test suite" ${PROJECT_IS_TOP_LEVEL})
message(STATUS "Dalia - DALIA_BUILD_SANDBOX=${DALIA_BUILD_TESTS}")

# Profiling
if (TRACY_ENABLE)
    set(DALIA_PROFILING_DEFAULT ON)
else()
    set(DALIA_PROFILE_DEFAULT OFF)
endif()

option(DALIA_PROFILE_ENABLE "Enable Tracy profiling in Dalia Engine" ${DALIA_PROFILING_DEFAULT})
message(STATUS "Dalia - DALIA_PROFILE_ENABLE=${DALIA_PROFILE_ENABLE}")

set(DALIA_TRACY_INCLUDE_DIR "" CACHE PATH "Optional path to host project's Tracy include directory")

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
