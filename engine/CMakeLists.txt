include(FetchContent)

# miniaudio
FetchContent_Declare(
    miniaudio
    URL "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(miniaudio)
set(MINIAUDIO_INCLUDE_DIR ${miniaudio_SOURCE_DIR})

# stb_vorbis
FetchContent_Declare(
    stb_vorbis
    URL "https://raw.githubusercontent.com/nothings/stb/master/stb_vorbis.c"
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(stb_vorbis)
set(STB_VORBIS_INCLUDE_DIR ${stb_vorbis_SOURCE_DIR})

set(ENGINE_FILES
    # Public headers
    include/dalia.h
    include/dalia/audio/Engine.h
    include/dalia/audio/PlaybackControl.h
    include/dalia/Core/LogLevel.h
    include/dalia/Core/Result.h

    # Internal headers and cpp-files
    src/Engine.cpp
    src/backend/miniaudio_impl.cpp
    src/backend/stb_vorbis_impl.cpp
    src/messaging/RtCommandQueue.h
    src/messaging/RtCommandQueue.cpp
    src/messaging/RtEventQueue.h
    src/messaging/RtEventQueue.cpp
    src/messaging/IoStreamRequestQueue.cpp
    src/messaging/IoStreamRequestQueue.h
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/FixedStack.h
    src/core/SPSCRingBuffer.h
    src/core/MPSCRingBuffer.h
    src/mixer/Voice.h
    src/mixer/StreamContext.h
    src/mixer/Bus.cpp
    src/mixer/Bus.h
    src/systems/RtSystem.cpp
    src/systems/RtSystem.h
    src/systems/IoStreamSystem.cpp
    src/systems/IoStreamSystem.h
    src/mixer/BusGraphCompiler.cpp
    src/mixer/BusGraphCompiler.h
    src/messaging/IoLoadRequestQueue.cpp
    src/messaging/IoLoadRequestQueue.h
    src/messaging/IoLoadEventQueue.cpp
    src/messaging/IoLoadEventQueue.h
    src/core/Constants.h
    src/systems/IoLoadSystem.cpp
    src/systems/IoLoadSystem.h
    src/resources/ResidentSound.h
    include/dalia/audio/ResourceHandle.h
    src/core/ResourcePool.h
    src/resources/AssetRegistry.cpp
    src/resources/AssetRegistry.h
    src/resources/StreamSound.h
    src/core/States.h
    src/core/Utility.cpp
    src/core/Utility.h
    include/dalia/audio/SoundHandle.h
)

add_library(dalia_engine STATIC ${ENGINE_FILES})

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
    SYSTEM PRIVATE ${MINIAUDIO_INCLUDE_DIR}
    SYSTEM PRIVATE ${STB_VORBIS_INCLUDE_DIR}
)

target_link_libraries(dalia_engine
    PRIVATE dalia_common
)

