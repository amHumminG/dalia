include(FetchContent)

FetchContent_Declare(
    miniaudio
    URL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(miniaudio)
set(MINIAUDIO_INCLUDE_DIR ${miniaudio_SOURCE_DIR})

set(ENGINE_FILES
    # Public headers
    include/dalia/AudioEngine.h
    include/dalia/AudioHandle.h
    include/dalia/LogLevel.h
    include/dalia/Result.h

    # Internal headers and cpp-files
    src/miniaudio_impl.cpp
    src/AudioEngine.cpp
    src/AudioCommandQueue.h
    src/AudioCommandQueue.cpp
    src/AudioEventQueue.h
    src/AudioEventQueue.cpp
    src/IoRequestQueue.cpp
    src/IoRequestQueue.h
    src/Logger.h
    src/Logger.cpp
    src/SPSCRingBuffer.h
    src/MPSCRingBuffer.h
    src/Voice.h
    src/StreamingContext.h
    src/Bus.cpp
    src/Bus.h
)

add_library(dalia_engine STATIC ${ENGINE_FILES})

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
    PRIVATE ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(dalia_engine
    PRIVATE dalia_common
)

