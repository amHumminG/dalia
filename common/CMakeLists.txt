set(COMMON_FILES
    src/Temp.h
    src/Temp.cpp
)

add_library(dalia_common STATIC ${COMMON_FILES})

target_include_directories(dalia_common
    PUBLIC src
)