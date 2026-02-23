include(FetchContent)

FetchContent_Declare(
        ffmpeg
        URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.0-latest-win64-gpl-shared-8.0.zip
)

FetchContent_MakeAvailable(ffmpeg)

add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
set_target_properties(FFmpeg::FFmpeg PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_SOURCE_DIR}/include"
        INTERFACE_LINK_DIRECTORIES    "${ffmpeg_SOURCE_DIR}/lib"
        INTERFACE_LINK_LIBRARIES      "avcodec;avformat;avutil;swscale;swresample"
)

#output
add_executable(dalia_studio src/main.cpp)

target_link_libraries(dalia_studio
        PRIVATE dalia_engine
        PRIVATE FFmpeg::FFmpeg
)

# Automatically copy the FFmpeg DLLs to the output folder after building
add_custom_command(TARGET dalia_studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ffmpeg_SOURCE_DIR}/bin"
        "$<TARGET_FILE_DIR:dalia_studio>"
)

# Copy existing assets directory to .exe directory (maybe not great if the directory is big)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET dalia_studio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:dalia_studio>/assets"
            COMMENT "Copying assets to build directory..."
    )
else()
    message(STATUS "No assets directory found. Skipping copy.")
endif()