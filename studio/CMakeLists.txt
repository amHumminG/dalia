include(FetchContent)

if(NOT DEFINED BUILD_EXAMPLES)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
)
FetchContent_Declare(
    ffmpeg
    URL https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.0-latest-win64-gpl-shared-8.0.zip
)
FetchContent_MakeAvailable(ffmpeg raylib imgui rlimgui)

# Define ImGui and rlImGui as libraries
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)
target_include_directories(imgui_lib SYSTEM PUBLIC ${imgui_SOURCE_DIR})

add_library(rlimgui_lib STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib SYSTEM PUBLIC ${rlimgui_SOURCE_DIR})
target_link_libraries(rlimgui_lib PUBLIC raylib imgui_lib)

add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
set_target_properties(FFmpeg::FFmpeg PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_SOURCE_DIR}/include"
    INTERFACE_LINK_DIRECTORIES    "${ffmpeg_SOURCE_DIR}/lib"
    INTERFACE_LINK_LIBRARIES      "avcodec;avformat;avutil;swscale;swresample"
)

set(STUDIO_FILES
    src/main.cpp
    src/Transcoder.h
    src/Transcoder.cpp
    src/app/Application.cpp
    src/app/Application.h
    src/editor/Panel.h
    src/commands/Command.h
    src/commands/CommandStack.cpp
    src/commands/CommandStack.h
    src/project/Project.cpp
    src/project/Project.h
    src/editor/SelectionContext.h
    src/editor/BrowserPanel.cpp
    src/editor/BrowserPanel.h
)

add_executable(dalia_studio ${STUDIO_FILES})
target_include_directories(dalia_studio
    PRIVATE src
)
target_link_libraries(dalia_studio
    PRIVATE dalia_engine
    PRIVATE dalia_common
    PRIVATE rlimgui_lib
    PRIVATE raylib
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