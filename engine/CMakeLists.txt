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
    include/dalia/dalia.h
    include/dalia/Engine.h
    include/dalia/PlaybackControl.h
    include/dalia/SoundControl.h
    include/dalia/EffectControl.h
    include/dalia/DeviceControl.h
    include/dalia/LogLevel.h
    include/dalia/Result.h

    # Internal headers and cpp-files
    src/engine/EngineInternalState.cpp
    src/engine/EngineInternalState.h
    src/engine/Engine.cpp
    src/engine/EngineSettings.cpp
    src/engine/EngineDevice.cpp
    src/engine/EngineAsset.cpp
    src/engine/EngineBus.cpp
    src/engine/EngineEffect.cpp
    src/engine/EnginePlayback.cpp
    src/engine/EngineListener.cpp

    src/core/Constants.h
    src/core/Logger.h
    src/core/Logger.cpp
    src/core/FixedStack.h
    src/core/FixedQueue.h
    src/core/SPSCRingBuffer.h
    src/core/SPSCRingBufferPCM.h
    src/core/MPSCRingBuffer.h
    src/core/ResourcePool.h
    src/core/Types.h
    src/core/Math.h
    src/core/StateSyncPool.h
    src/core/AsyncPool.h
    src/core/ParameterBridge.h
    src/core/Speakers.h

    src/async/AsyncMessageBus.h
    src/async/AsyncMessageBus.cpp
    src/async/WakeableThread.cpp
    src/async/WakeableThread.h
    src/async/IntervalThread.cpp
    src/async/IntervalThread.h
    src/async/messaging/AsyncMixerMessaging.h
    src/async/messaging/AsyncMixerMessaging.cpp
    src/async/messaging/AsyncStreamMessaging.h
    src/async/messaging/AsyncLoadMessaging.h
    src/async/messaging/AsyncControlMessaging.h

    src/mixer/MixController.cpp
    src/mixer/MixController.h
    src/mixer/MixControllerPlayback.cpp
    src/mixer/MixControllerListener.cpp
    src/mixer/MixControllerBus.cpp
    src/mixer/MixControllerEffect.cpp
    src/mixer/Mixer.cpp
    src/mixer/Mixer.h
    src/mixer/MixGraphCompiler.cpp
    src/mixer/MixGraphCompiler.h
    src/mixer/processors/EffectProcessor.cpp
    src/mixer/processors/EffectProcessor.h
    src/mixer/processors/AcousticProcessor.cpp
    src/mixer/processors/AcousticProcessor.h
    src/mixer/processors/BusProcessor.cpp
    src/mixer/processors/BusProcessor.h
    src/mixer/processors/VoiceProcessor.cpp
    src/mixer/processors/VoiceProcessor.h
    src/mixer/nodes/Voice.h
    src/mixer/nodes/Listener.h
    src/mixer/nodes/StreamContext.h
    src/mixer/nodes/Bus.h
    src/mixer/nodes/Effect.h
    src/mixer/nodes/EffectChain.h
    src/mixer/nodes/effects/EffectTraits.h
    src/mixer/nodes/effects/Biquad.h
    src/mixer/nodes/effects/Biquad.cpp
    src/mixer/dsp/Resampler.h
    src/mixer/dsp/Resampler.cpp
    src/mixer/dsp/PeakLimiter.h
    src/mixer/dsp/PeakLimiter.cpp
    src/mixer/dsp/Slew.h
    src/mixer/dsp/Slew.cpp
    src/mixer/dsp/Buffer.h
    src/mixer/dsp/Gain.h
    src/mixer/dsp/Spatial.h

    src/asset/AssetRegistry.cpp
    src/asset/AssetRegistry.h
    src/asset/AssetController.cpp
    src/asset/AssetController.h
    src/asset/SoundDescriptor.h
    src/asset/types/ResidentSound.h
    src/asset/types/StreamSound.h

    src/io/codec/Codec.h
    src/io/codec/VorbisCodec.h
    src/io/codec/VorbisCodec.cpp
    src/io/codec/Codec.cpp
    src/io/IoController.cpp
    src/io/IoController.h

    src/platform/DeviceController.h
    src/platform/DeviceController.cpp
    src/platform/AudioBackend.h
    src/platform/NullOutputDevice.h
    src/platform/NullOutputDevice.cpp
    src/platform/OutputDevice.h
    src/platform/HighResTimer.h
    src/platform/PlatformThread.h
)

if (WIN32)
    list(APPEND ENGINE_FILES
        src/platform/windows/WasapiAudioBackend.cpp
        src/platform/windows/WasapiAudioBackend.h
        src/platform/windows/WasapiOutputDevice.cpp
        src/platform/windows/WasapiOutputDevice.h
        src/platform/windows/HighResTimer.cpp
        src/platform/windows/PlatformThread.cpp
    )
elseif (APPLE)
    list(APPEND ENGINE_FILES
        # Not supported
    )
elseif (UNIX AND NOT APPLE)
    list(APPEND ENGINE_FILES
        # Not supported
    )
endif()

add_library(dalia_engine STATIC ${ENGINE_FILES})
dalia_treat_warnings_as_errors(dalia_engine)

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
)

target_sources(dalia_engine
    PRIVATE $<TARGET_OBJECTS:stb_vorbis_lib>
)

target_link_libraries(dalia_engine
    PRIVATE dalia_common
    PRIVATE stb_vorbis_lib
)

if (WIN32)
    target_link_libraries(dalia_engine
        PRIVATE ole32 # Backend (COM)
        PRIVATE avrt  # Backend (MMCSS)
    )
endif()

add_library(dalia::engine ALIAS dalia_engine)

include(GNUInstallDirs)
install(TARGETS dalia_engine
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
