# Treat warnings as errors (should work for most compilers)
function (dalia_treat_warnings_as_errors target_name)
    if(MSVC)
        target_compile_options(${target_name}
            PRIVATE
            /W4 /WX /wd4324
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name}
            PRIVATE
            -Wall -Wextra -Werror -Wpedantic -Wshadow
        )
    endif()
endfunction()

add_subdirectory(vendor/stb_vorbis)

# Source files
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

    src/backend/WasapiBackend.cpp
    src/backend/WasapiBackend.h
    src/backend/IAudioBackend.h

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
    src/core/HandleManager.h
    src/core/HandleManager.cpp
    src/core/StateSyncPool.h
    src/core/AsyncPool.h
    src/core/ParameterBridge.h

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
    src/mixer/effects/Biquad.h
    src/mixer/effects/Biquad.cpp
    src/mixer/Listener.h
    src/mixer/Speakers.h
    src/mixer/Resampler.h
    src/mixer/Resampler.cpp
    src/mixer/PeakLimiter.h
    src/mixer/PeakLimiter.cpp

    src/dsp/Slew.h
    src/dsp/Slew.cpp

    src/resources/ResidentSound.h
    src/resources/AssetRegistry.cpp
    src/resources/AssetRegistry.h
    src/resources/StreamSound.h
)

add_library(dalia_engine STATIC ${ENGINE_FILES})
dalia_treat_warnings_as_errors(dalia_engine)

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
)

target_link_libraries(dalia_engine
    PRIVATE dalia_common
    PRIVATE stb_vorbis_lib
    PRIVATE ole32 # Backend (COM initialization)
    PRIVATE avrt  # Backend (MMCSS)
)

add_library(dalia::engine ALIAS dalia_engine)
