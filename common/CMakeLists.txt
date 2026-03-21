set(COMMON_FILES
    include/common/Soundbank.h
    include/common/StringID.h
    src/SoundbankWriter.cpp
    include/common/SoundbankWriter.h
)

add_library(dalia_common STATIC ${COMMON_FILES})

target_include_directories(dalia_common
    PUBLIC include
    PRIVATE src
)