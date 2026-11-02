cmake_minimum_required(VERSION 3.24)
project(CursorBeacon LANGUAGES CXX)

enable_language(RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CURSORBEACON_DEFINITIONS
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000000)

add_library(cursorbeacon_core STATIC
    src/core/shake_detector.cpp
    src/core/scale_controller.cpp
    src/core/state_machine.cpp
    src/core/fullscreen_rules.cpp)
target_include_directories(cursorbeacon_core PUBLIC src)
target_compile_definitions(cursorbeacon_core PUBLIC ${CURSORBEACON_DEFINITIONS})

if(MSVC)
    target_compile_options(cursorbeacon_core PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(cursorbeacon_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cursorbeacon
    src/main.cpp
    src/app/controller.cpp
    src/app/self_test.cpp
    src/platform/monitors.cpp
    src/platform/fullscreen.cpp
    src/platform/pointer_source.cpp
    src/platform/cursor_theme.cpp
    src/platform/cursor_renderer.cpp
    src/platform/cursor_effector.cpp
    src/config/config.cpp
    src/lifecycle/single_instance.cpp
    src/lifecycle/service_control.cpp
    src/lifecycle/crash_guard.cpp
    src/util/log.cpp
    src/util/time.cpp
    resources/app.rc)

target_include_directories(cursorbeacon PRIVATE src resources)
target_compile_definitions(cursorbeacon PRIVATE ${CURSORBEACON_DEFINITIONS})
target_link_libraries(cursorbeacon PRIVATE
    cursorbeacon_core
    user32 gdi32 shcore dwmapi shell32 ole32 wtsapi32 advapi32)

if(MSVC)
    target_compile_options(cursorbeacon PRIVATE /W4 /permissive- /utf-8 /EHsc)
    set_property(TARGET cursorbeacon PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(cursorbeacon PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(cursorbeacon PRIVATE -static $<$<CONFIG:Release>:-s>)
endif()

enable_testing()

add_executable(cursorbeacon_tests
    tests/test_main.cpp
    tests/test_shake_detector.cpp
    tests/test_scale_controller.cpp
    tests/test_state_machine.cpp
    tests/test_fullscreen_rules.cpp
    resources/app.rc)
target_include_directories(cursorbeacon_tests PRIVATE src third_party resources)
target_link_libraries(cursorbeacon_tests PRIVATE cursorbeacon_core)
target_compile_definitions(cursorbeacon_tests PRIVATE ${CURSORBEACON_DEFINITIONS})

if(MSVC)
    target_compile_options(cursorbeacon_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(cursorbeacon_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(cursorbeacon_tests PRIVATE -static)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(cursorbeacon_tests PRIVATE -Wno-c2y-extensions)
    endif()
endif()

add_test(NAME unit COMMAND cursorbeacon_tests)
