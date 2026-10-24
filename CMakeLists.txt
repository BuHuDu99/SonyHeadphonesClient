cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(SonyHeadphonesClient CXX)
add_definitions(-D__HEADPHONES_APP_VERSION__="1.4.4")

set(CMAKE_CXX_STANDARD 20)

find_package(Threads REQUIRED)

if (WIN32)
    add_executable(SonyHeadphonesClient WIN32)
elseif(UNIX AND NOT APPLE)
    add_executable(SonyHeadphonesClient)
elseif(APPLE)
    add_executable(SonyHeadphonesClient MACOSX_BUNDLE)
    set_target_properties(SonyHeadphonesClient PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/macos/Info.plist
    )
endif()

target_sources(SonyHeadphonesClient
    PRIVATE src/BluetoothWrapper.cpp
    src/CommandSerializer.cpp
    src/App.cpp
    src/AppConfig.cpp
    src/Headphones.cpp
    contrib/tomlplusplus/src/toml.cpp
)

target_include_directories(SonyHeadphonesClient
    PRIVATE contrib/tomlplusplus/include
)

target_include_directories(SonyHeadphonesClient
    PRIVATE src contrib
)

set (DEAR_IMGUI TRUE)

if (DEAR_IMGUI)
    target_sources(SonyHeadphonesClient
        PRIVATE
        contrib/imgui/imgui.cpp
        contrib/imgui/imgui_draw.cpp
        contrib/imgui/imgui_tables.cpp
        contrib/imgui/imgui_widgets.cpp
        contrib/imgui/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories (SonyHeadphonesClient
        PRIVATE contrib/imgui
    )
endif ()

if (UNIX AND NOT APPLE)
    # Disable Wayland support
    # GLFW doesn't respect GLFW_SCALE_TO_MONITOR on wayland - and there's seemingly no
    # easy way to scale them ourselves.
    # https://github.com/glfw/glfw/issues/2527 also mentions this.
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    add_subdirectory(contrib/glfw)
    target_link_libraries(SonyHeadphonesClient PRIVATE glfw)
    target_sources(SonyHeadphonesClient
        PRIVATE src/Main.cpp
        src/platform/linux/DBusHelper.cpp
        src/platform/linux/LinuxBluetoothConnector.cpp
        contrib/imgui/backends/imgui_impl_opengl3.cpp
        contrib/imgui/backends/imgui_impl_glfw.cpp
    )

    set (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/src/platform/linux)
    set (OpenGL_GL_PREFERENCE GLVND)

    find_package(DBus REQUIRED)
    find_package(OpenGL REQUIRED)

    if ((NOT DBUS_FOUND) OR (NOT OPENGL_FOUND))
        message ( FATAL_ERROR
            "Didn't find one of the packages. Refer to the README for a list of dependencies that you need to build this application."
        )
    endif ()

    target_include_directories(SonyHeadphonesClient
        PRIVATE ${DBUS_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
    )

    target_link_libraries(SonyHeadphonesClient
        PRIVATE ${DBUS_LIBRARIES}
        glfw
        bluetooth
        OpenGL::GL
    )
endif ()

if (WIN32)
    add_definitions(-DUNICODE)
    add_subdirectory(contrib/glfw)
    target_link_libraries(SonyHeadphonesClient PRIVATE glfw)

    target_sources(SonyHeadphonesClient
        PRIVATE src/Main.cpp
        src/platform/windows/WindowsBluetoothConnector.cpp
        contrib/imgui/backends/imgui_impl_opengl3.cpp
        contrib/imgui/backends/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/app.manifest
    )
    # https://devblogs.microsoft.com/oldnewthing/20240816-00/?p=110136
    target_link_libraries(SonyHeadphonesClient
        PRIVATE opengl32 comctl32
    )
endif ()

if (APPLE)
    enable_language(OBJCXX)
    # Homebrew does not offer universal binaries for GLFW, so we need to build it ourselves
    add_subdirectory(contrib/glfw)
    target_link_libraries(SonyHeadphonesClient PRIVATE glfw)

    target_sources(SonyHeadphonesClient
        PRIVATE src/Main.cpp
        src/platform/macos/MacOSBluetoothConnector.mm
        src/platform/macos/MacOSNowPlayingProvider.mm
        contrib/imgui/backends/imgui_impl_metal.mm
        contrib/imgui/backends/imgui_impl_glfw.cpp
    )
    set_source_files_properties(src/Main.cpp
        PROPERTIES LANGUAGE OBJCXX
    )
    # IOBluetooth returns a mix of framework-owned and caller-owned Objective-C
    # objects. Use ARC for this backend to avoid over-release during failed SDP/
    # RFCOMM connection attempts. Keep the ImGui Metal backend's own setting.
    set_source_files_properties(src/platform/macos/MacOSBluetoothConnector.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
    set_source_files_properties(src/platform/macos/MacOSNowPlayingProvider.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
    set(MACOS_APP_RESOURCES src/platform/macos/Resources/AppIcon.icns LICENSE THIRD_PARTY_NOTICES.txt)
    set_source_files_properties(${MACOS_APP_RESOURCES}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(SonyHeadphonesClient PRIVATE ${MACOS_APP_RESOURCES})
    target_link_libraries(SonyHeadphonesClient
        PRIVATE "-framework Foundation -framework IOBluetooth -framework IOKit -framework Metal -framework MetalKit -framework Cocoa -framework CoreVideo -framework QuartzCore"
    )
endif ()

target_link_libraries(SonyHeadphonesClient
    PRIVATE Threads::Threads
)

include(CTest)
if (BUILD_TESTING)
    add_executable(EqualizerSerializerTests
        tests/EqualizerSerializerTests.cpp
        src/CommandSerializer.cpp
    )
    target_include_directories(EqualizerSerializerTests PRIVATE src)
    add_test(NAME EqualizerSerializerTests COMMAND EqualizerSerializerTests)

    add_executable(ProtocolSafetyTests
        tests/ProtocolSafetyTests.cpp
        src/CommandSerializer.cpp
        src/BluetoothWrapper.cpp
        src/Headphones.cpp
    )
    target_include_directories(ProtocolSafetyTests PRIVATE src)
    target_link_libraries(ProtocolSafetyTests PRIVATE Threads::Threads)
    add_test(NAME ProtocolSafetyTests COMMAND ProtocolSafetyTests)

    add_executable(BluetoothTransactionTests
        tests/BluetoothTransactionTests.cpp src/CommandSerializer.cpp src/BluetoothWrapper.cpp src/Headphones.cpp)
    target_include_directories(BluetoothTransactionTests PRIVATE src)
    target_link_libraries(BluetoothTransactionTests PRIVATE Threads::Threads)
    add_test(NAME BluetoothTransactionTests COMMAND BluetoothTransactionTests)

    add_executable(MenuBarDisplayTests tests/MenuBarDisplayTests.cpp src/AppConfig.cpp contrib/tomlplusplus/src/toml.cpp)
    target_include_directories(MenuBarDisplayTests PRIVATE src contrib/tomlplusplus/include)
    add_test(NAME MenuBarDisplayTests COMMAND MenuBarDisplayTests)

    add_executable(EqualizerPreviewTests tests/EqualizerPreviewTests.cpp src/AppConfig.cpp contrib/tomlplusplus/src/toml.cpp)
    target_include_directories(EqualizerPreviewTests PRIVATE src contrib/tomlplusplus/include)
    add_test(NAME EqualizerPreviewTests COMMAND EqualizerPreviewTests)

    add_executable(SonyBatteryTests tests/SonyBatteryTests.cpp
        src/CommandSerializer.cpp src/BluetoothWrapper.cpp src/Headphones.cpp)
    target_link_libraries(SonyBatteryTests PRIVATE Threads::Threads)
    target_include_directories(SonyBatteryTests PRIVATE src)
    add_test(NAME SonyBatteryTests COMMAND SonyBatteryTests)
    # These standalone tests use assert; keep checks active in Release builds too.
    foreach(test_target EqualizerSerializerTests ProtocolSafetyTests BluetoothTransactionTests MenuBarDisplayTests EqualizerPreviewTests SonyBatteryTests)
        if (MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
    endforeach()
    set_tests_properties(ProtocolSafetyTests BluetoothTransactionTests EqualizerPreviewTests PROPERTIES TIMEOUT 30)
endif()
