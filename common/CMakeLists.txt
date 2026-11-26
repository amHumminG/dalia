set(COMMON_FILES
    include/common/Soundbank.hpp
    include/common/StringID.hpp
    src/SoundbankWriter.cpp
    include/common/SoundbankWriter.hpp
)

add_library(dalia_common STATIC ${COMMON_FILES})

target_include_directories(dalia_common
    PUBLIC include
    PRIVATE src
)