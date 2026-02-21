set(ENGINE_FILES
    # Public headers
    include/dalia/AudioEngine.h
    include/dalia/AudioHandle.h
    include/dalia/LogLevel.h
    include/dalia/Result.h

    # Internal headers and cpp-files
    src/AudioEngine.cpp
    src/CommandQueue.h
    src/CommandQueue.cpp
    src/EventQueue.h
    src/EventQueue.cpp
    src/Logger.h
    src/Logger.cpp
    src/SPSCRingBuffer.h
    src/MPSCRingBuffer.h

    # Libraries
    lib/miniaudio/miniaudio.h
)

add_library(dalia_engine STATIC ${ENGINE_FILES})

target_include_directories(dalia_engine 
    PUBLIC include
    PRIVATE src
    PRIVATE lib/miniaudio
)