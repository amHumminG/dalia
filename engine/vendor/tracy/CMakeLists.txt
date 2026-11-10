if (NOT DALIA_ENABLE_PROFILING)
    return()
endif()

# Only fetch Tracy for standalone builds
if (NOT TARGET Tracy::TracyClient)
    include(FetchContent)

    set(TRACY_ENABLE ON CACHE BOOL "Enable profiling" FORCE)
    option(TRACY_ON_DEMAND "On-demand profiling" ON)

    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.14.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(tracy)
endif()
