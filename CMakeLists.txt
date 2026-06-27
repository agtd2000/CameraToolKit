cmake_minimum_required(VERSION 3.16)
project(CameraToolKit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
set(wxWidgets_DIR "D:/vcpkg/installed/x64-windows/share/wxwidgets" CACHE PATH "wxWidgets config directory")

find_package(OpenCV REQUIRED)

find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base adv aui)

file(GLOB IMGUI_SOURCES
    third_party/imgui/*.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

set(CORE_SOURCES
    src/algo/dead_pixel.cpp
    src/algo/flat_field.cpp
    src/algo/white_balance.cpp
    src/algo/ccm.cpp
    src/algo/color_error.cpp
    src/algo/spectral.cpp
    src/algo/quick_color_calib.cpp
    src/algo/roi.cpp
    src/algo/gamma.cpp
    src/algo/clut.cpp
    src/algo/image_node.cpp
)

set(WXUI_SOURCES
    src/wxui/main_app.cpp
    src/wxui/main_frame.cpp
    src/wxui/image_canvas.cpp
    src/wxui/neumorphic_panel.cpp
    src/wxui/dead_pixel_panel.cpp
    src/wxui/flat_field_panel.cpp
    src/wxui/quick_color_calib_panel.cpp
    src/wxui/color_error_panel.cpp
    src/wxui/color_char_calib_panel.cpp
    src/wxui/spectral_calib_panel.cpp
    src/wxui/worker_thread.cpp
    src/utils/image_io.cpp
    src/utils/tone_mapping.cpp
    src/utils/logger.cpp
    src/utils/memory_pool.cpp
)

set(RESOURCE_FILES
    src/logo/app_icon.rc
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    ${CORE_SOURCES}
    ${WXUI_SOURCES}
    ${RESOURCE_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/wxui
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    wx::core wx::base wx::adv wx::aui
)

file(COPY ${CMAKE_SOURCE_DIR}/images DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/logo/logo.ico DESTINATION ${CMAKE_BINARY_DIR}/Release)



if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _USE_MATH_DEFINES
    )
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
