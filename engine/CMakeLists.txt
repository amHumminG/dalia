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
    include/dalia.h
    include/dalia/audio/AudioEngine.h
    include/dalia/audio/AudioControl.h
    include/dalia/Core/LogLevel.h
    include/dalia/Core/Result.h

    # Internal headers and cpp-files
    src/AudioEngine.cpp
    src/backend/miniaudio_impl.cpp
    src/messaging/AudioCommandQueue.h
    src/messaging/AudioCommandQueue.cpp
    src/messaging/AudioEventQueue.h
    src/messaging/AudioEventQueue.cpp
    src/messaging/IoRequestQueue.cpp
    src/messaging/IoRequestQueue.h
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/FixedStack.h
    src/core/SPSCRingBuffer.h
    src/core/MPSCRingBuffer.h
    src/mixer/Voice.h
    src/mixer/StreamingContext.h
    src/mixer/Bus.cpp
    src/mixer/Bus.h
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

