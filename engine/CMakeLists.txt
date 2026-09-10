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
    src/Engine.cpp

    src/backend/DeviceManager.h
    src/backend/OutputDevice.h
    src/backend/HighResTimer.h
    src/backend/PlatformThread.h

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

    src/async/AsyncStreamSystem.cpp
    src/async/AsyncStreamSystem.h
    src/async/AsyncLoadSystem.cpp
    src/async/AsyncLoadSystem.h
    src/async/AsyncControlSystem.cpp
    src/async/AsyncControlSystem.h

    src/messaging/RtMessaging.h
    src/messaging/RtMessaging.cpp
    src/messaging/AsyncStreamMessaging.h
    src/messaging/AsyncLoadMessaging.h
    src/messaging/AsyncControlMessaging.h

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

if (WIN32)
    list(APPEND ENGINE_FILES
        src/backend/windows/WindowsDeviceManager.cpp
        src/backend/windows/WindowsDeviceManager.h
        src/backend/windows/WasapiOutputDevice.cpp
        src/backend/windows/WasapiOutputDevice.h
        src/backend/windows/WindowsNullOutputDevice.cpp
        src/backend/windows/WindowsNullOutputDevice.h
        src/backend/windows/HighResTimer.cpp
        src/backend/windows/PlatformThread.cpp
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

target_link_libraries(dalia_engine
    PRIVATE dalia_common
    PRIVATE stb_vorbis_lib
)

if (WIN32)
    target_link_libraries(dalia_engine
        PRIVATE ole32 # Backend (COM initialization)
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
