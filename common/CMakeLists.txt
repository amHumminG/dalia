set(COMMON_FILES
    # Internal headers and cpp-files
    src/Soundbank.h
    src/StringID.h
    src/SoundbankWriter.cpp
    src/SoundbankWriter.h
)

add_library(dalia_common STATIC ${COMMON_FILES})

target_include_directories(dalia_common
    PUBLIC src
)