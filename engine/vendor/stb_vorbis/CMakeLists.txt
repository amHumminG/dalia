# Third-party libraries

include(FetchContent)

# stb_vorbis
FetchContent_Declare(
    stb_vorbis
    URL "https://raw.githubusercontent.com/nothings/stb/master/stb_vorbis.c"
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb_vorbis)

add_library(stb_vorbis_lib STATIC stb_vorbis_impl.cpp)
target_include_directories(stb_vorbis_lib SYSTEM PUBLIC ${stb_vorbis_SOURCE_DIR})