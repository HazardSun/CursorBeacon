cmake_minimum_required(VERSION 3.24)
project(ToolchainCheck LANGUAGES CXX)

enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CURSORBEACON_DEFINITIONS
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000000)

add_executable(toolchain-check main.cpp)
target_compile_definitions(toolchain-check PRIVATE ${CURSORBEACON_DEFINITIONS})
target_link_libraries(toolchain-check PRIVATE
    user32 gdi32 shcore dwmapi shell32 wtsapi32 advapi32 ole32)

add_executable(toolchain-check-gui WIN32 gui.cpp toolchain-check.rc)
target_compile_definitions(toolchain-check-gui PRIVATE ${CURSORBEACON_DEFINITIONS})
target_link_libraries(toolchain-check-gui PRIVATE user32)

if(MINGW)
    target_link_options(toolchain-check PRIVATE -static)
    target_link_options(toolchain-check-gui PRIVATE -static)
endif()

if(MSVC)
    target_compile_options(toolchain-check PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_options(toolchain-check-gui PRIVATE /W4 /permissive- /utf-8 /EHsc)
    set_property(TARGET toolchain-check PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET toolchain-check-gui PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(toolchain-check PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(toolchain-check-gui PRIVATE -Wall -Wextra -Wpedantic)
endif()
