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
    include/dalia/dalia.hpp
    include/dalia/Engine.hpp
    include/dalia/Playback.hpp
    include/dalia/Sound.hpp
    include/dalia/Effect.hpp
    include/dalia/Device.hpp
    include/dalia/Log.hpp
    include/dalia/Result.hpp

    # Internal headers and cpp-files
    src/engine/EngineInternalState.cpp
    src/engine/EngineInternalState.hpp
    src/engine/Engine.cpp
    src/engine/EngineSettings.cpp
    src/engine/EngineDevice.cpp
    src/engine/EngineAsset.cpp
    src/engine/EngineBus.cpp
    src/engine/EngineEffect.cpp
    src/engine/EnginePlayback.cpp
    src/engine/EngineListener.cpp

    src/core/Constants.hpp
    src/core/Logger.hpp
    src/core/Logger.cpp
    src/core/FixedStack.hpp
    src/core/FixedQueue.hpp
    src/core/SPSCRingBuffer.hpp
    src/core/SPSCRingBufferPCM.hpp
    src/core/MPSCRingBuffer.hpp
    src/core/ResourcePool.hpp
    src/core/Types.hpp
    src/core/Math.hpp
    src/core/StateSyncPool.hpp
    src/core/AsyncPool.hpp
    src/core/ParameterBridge.hpp
    src/core/Speakers.hpp
    src/core/Profiler.hpp
    src/core/Assert.hpp

    src/async/AsyncMessageBus.hpp
    src/async/AsyncMessageBus.cpp
    src/async/WakeableThread.cpp
    src/async/WakeableThread.hpp
    src/async/IntervalThread.cpp
    src/async/IntervalThread.hpp
    src/async/messaging/AsyncMixerMessaging.hpp
    src/async/messaging/AsyncMixerMessaging.cpp
    src/async/messaging/AsyncStreamMessaging.hpp
    src/async/messaging/AsyncLoadMessaging.hpp
    src/async/messaging/AsyncControlMessaging.hpp

    src/mixer/MixController.cpp
    src/mixer/MixController.hpp
    src/mixer/MixControllerPlayback.cpp
    src/mixer/MixControllerListener.cpp
    src/mixer/MixControllerBus.cpp
    src/mixer/MixControllerEffect.cpp
    src/mixer/Mixer.cpp src/mixer/Mixer.hpp
    src/mixer/MixGraphCompiler.cpp
    src/mixer/MixGraphCompiler.hpp
    src/mixer/processors/EffectProcessor.cpp
    src/mixer/processors/EffectProcessor.hpp
    src/mixer/processors/AcousticProcessor.cpp
    src/mixer/processors/AcousticProcessor.hpp
    src/mixer/processors/BusProcessor.cpp
    src/mixer/processors/BusProcessor.hpp
    src/mixer/processors/VoiceProcessor.cpp
    src/mixer/processors/VoiceProcessor.hpp
    src/mixer/nodes/Voice.hpp
    src/mixer/nodes/Listener.hpp
    src/mixer/nodes/StreamContext.hpp
    src/mixer/nodes/Bus.hpp
    src/mixer/nodes/Effect.hpp
    src/mixer/nodes/EffectChain.hpp
    src/mixer/nodes/effects/EffectTraits.hpp
    src/mixer/nodes/effects/Biquad.hpp
    src/mixer/nodes/effects/Biquad.cpp
    src/mixer/dsp/Resampler.hpp
    src/mixer/dsp/Resampler.cpp
    src/mixer/dsp/PeakLimiter.hpp
    src/mixer/dsp/PeakLimiter.cpp
    src/mixer/dsp/Slew.hpp
    src/mixer/dsp/Slew.cpp
    src/mixer/dsp/Buffer.hpp
    src/mixer/dsp/Gain.hpp
    src/mixer/dsp/Spatial.hpp

    src/asset/AssetRegistry.cpp
    src/asset/AssetRegistry.hpp
    src/asset/AssetController.cpp
    src/asset/AssetController.hpp
    src/asset/SoundDescriptor.hpp
    src/asset/types/ResidentSound.hpp
    src/asset/types/StreamSound.hpp

    src/io/codec/Codec.hpp
    src/io/codec/VorbisCodec.hpp
    src/io/codec/VorbisCodec.cpp
    src/io/codec/Codec.cpp
    src/io/IoController.cpp
    src/io/IoController.hpp

    src/platform/DeviceController.hpp
    src/platform/DeviceController.cpp
    src/platform/AudioBackend.hpp
    src/platform/NullOutputDevice.hpp
    src/platform/NullOutputDevice.cpp
        src/platform/OutputDevice.hpp
    src/platform/HighResTimer.hpp
    src/platform/PlatformThread.hpp
        src/mixer/nodes/effects/FdnReverb.hpp
        src/mixer/nodes/effects/FdnReverb.cpp
)

if (WIN32)
    list(APPEND ENGINE_FILES
        src/platform/windows/WasapiAudioBackend.cpp
        src/platform/windows/WasapiAudioBackend.hpp
        src/platform/windows/WasapiOutputDevice.cpp
        src/platform/windows/WasapiOutputDevice.hpp
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

if (DALIA_PROFILE_ENABLE)
    target_compile_definitions(dalia_engine PUBLIC DALIA_PROFILE_ENABLE)

    if (DALIA_TRACY_INCLUDE_DIR)
        # External non-CMake host -> Use headers and definitions directly
        target_include_directories(dalia_engine PUBLIC "${DALIA_TRACY_INCLUDE_DIR}")
        target_compile_definitions(dalia_engine PUBLIC TRACY_ENABLE)

        if (TRACY_ON_DEMAND)
            target_compile_definitions(dalia_engine PUBLIC TRACY_ON_DEMAND)
        endif()
        if (TRACY_FIBERS)
            target_compile_definitions(dalia_engine PUBLIC TRACY_FIBERS)
        endif()
    else()
        # Standalone CMake or CMake subprojects
        if (NOT TARGET Tracy::TracyClient)
            add_subdirectory(vendor/tracy)
        endif()
        target_link_libraries(dalia_engine PUBLIC Tracy::TracyClient)
    endif()
endif()

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
)

target_link_libraries(dalia_engine
    PRIVATE dalia_common
    PRIVATE stb_vorbis_lib
)

if (WIN32)
    target_compile_definitions(dalia_engine PUBLIC NOMINMAX)

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
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
