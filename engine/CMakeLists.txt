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
    include/dalia/audio/SoundControl.h
    include/dalia/audio/EffectControl.h
    include/dalia/Core/LogLevel.h
    include/dalia/Core/Result.h

    # Internal headers and cpp-files
    src/Engine.cpp

    src/backend/miniaudio_impl.cpp
    src/backend/stb_vorbis_impl.cpp

    src/core/Constants.h
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/FixedStack.h
    src/core/SPSCRingBuffer.h
    src/core/MPSCRingBuffer.h
    src/core/ResourcePool.h
    src/core/Utility.cpp
    src/core/Utility.h
    src/core/Types.h
    src/core/Math.h

    src/io/IoStreamSystem.cpp
    src/io/IoStreamSystem.h
    src/io/IoLoadSystem.cpp
    src/io/IoLoadSystem.h


    src/messaging/RtCommandQueue.h
    src/messaging/RtCommandQueue.cpp
    src/messaging/RtEventQueue.h
    src/messaging/RtEventQueue.cpp
    src/messaging/IoStreamRequestQueue.cpp
    src/messaging/IoStreamRequestQueue.h
    src/messaging/IoLoadRequestQueue.cpp
    src/messaging/IoLoadRequestQueue.h
    src/messaging/IoLoadEventQueue.cpp
    src/messaging/IoLoadEventQueue.h

    src/mixer/Voice.h
    src/mixer/StreamContext.h
    src/mixer/Bus.h
    src/mixer/RtSystem.cpp
    src/mixer/RtSystem.h
    src/mixer/MixGraphCompiler.cpp
    src/mixer/MixGraphCompiler.h
    src/mixer/effects/BiquadFilter.h
    src/mixer/effects/BiquadFilter.cpp

    src/resources/ResidentSound.h
    src/resources/AssetRegistry.cpp
    src/resources/AssetRegistry.h
    src/resources/StreamSound.h
    src/core/HandleManager.h
    src/core/HandleManager.cpp
    src/backend/WasapiBackend.cpp
    src/backend/WasapiBackend.h
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

