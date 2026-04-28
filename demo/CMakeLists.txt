include(FetchContent)

if(NOT DEFINED BUILD_EXAMPLES)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
endif()

# Fetch external dependencies
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.6
)
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
)
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG master
)
FetchContent_MakeAvailable(raylib imgui rlimgui imguizmo)

# Define ImGui and rlImGui as libraries
add_library(imgui_lib_demo STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)
target_include_directories(imgui_lib_demo SYSTEM PUBLIC ${imgui_SOURCE_DIR})

add_library(rlimgui_lib_demo STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib_demo SYSTEM PUBLIC ${rlimgui_SOURCE_DIR})
target_link_libraries(rlimgui_lib_demo PUBLIC raylib imgui_lib_demo)

add_library(imguizmo_lib STATIC
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)
target_include_directories(imguizmo_lib SYSTEM PUBLIC ${imguizmo_SOURCE_DIR})
target_link_libraries(imguizmo_lib PUBLIC imgui_lib_demo)

SET(DEMO_FILES
    src/main.cpp
    src/Sandbox.cpp
    src/Sandbox.h
    src/Panel.h
    src/PlaybackInstance.cpp
    src/PlaybackInstance.h
    src/Listener.cpp
    src/Listener.h
    src/SoundAsset.cpp
    src/SoundAsset.h
    src/MixingBus.cpp
    src/MixingBus.h
    src/UI.h
    src/DrawHelper.h
    src/DrawHelper.cpp
)

add_executable(dalia_demo ${DEMO_FILES})

target_link_libraries(dalia_demo
    PRIVATE dalia_engine
    PRIVATE rlimgui_lib_demo
    PRIVATE imguizmo_lib
    PRIVATE raylib
)

# Copy existing assets directory to .exe directory (maybe not great if the directory is big)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET dalia_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:dalia_demo>/assets"
        COMMENT "Copying assets to build directory..."
    )
else()
    message(STATUS "No assets directory found. Skipping copy.")
endif()