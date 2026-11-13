cmake_minimum_required(VERSION 3.24)
project(home-theatre-launcher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HTL_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick DBus Network Svg Test)
# Idle-screen video (QML import only; also silences the plugin-link warning).
find_package(Qt6 6.8 REQUIRED COMPONENTS Multimedia)
find_package(LayerShellQt REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET wayland-client)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(WLR_PROTOCOLS_DIR "" CACHE PATH "Directory containing wlr-protocols XML files")
set(FOREIGN_TOPLEVEL_XML "${WLR_PROTOCOLS_DIR}/unstable/wlr-foreign-toplevel-management-unstable-v1.xml")
if(NOT EXISTS "${FOREIGN_TOPLEVEL_XML}")
    message(FATAL_ERROR "wlr-foreign-toplevel protocol not found; set WLR_PROTOCOLS_DIR")
endif()

set(PROTO_DIR "${CMAKE_CURRENT_BINARY_DIR}/protocols")
file(MAKE_DIRECTORY "${PROTO_DIR}")
set(PROTO_HEADER "${PROTO_DIR}/wlr-foreign-toplevel-management-unstable-v1-client-protocol.h")
set(PROTO_CODE "${PROTO_DIR}/wlr-foreign-toplevel-management-unstable-v1-protocol.c")
add_custom_command(OUTPUT "${PROTO_HEADER}"
    COMMAND ${WAYLAND_SCANNER} client-header "${FOREIGN_TOPLEVEL_XML}" "${PROTO_HEADER}"
    DEPENDS "${FOREIGN_TOPLEVEL_XML}")
add_custom_command(OUTPUT "${PROTO_CODE}"
    COMMAND ${WAYLAND_SCANNER} private-code "${FOREIGN_TOPLEVEL_XML}" "${PROTO_CODE}"
    DEPENDS "${FOREIGN_TOPLEVEL_XML}")

qt_standard_project_setup(REQUIRES 6.8)

# Everything except main(), shared with the tests.
qt_add_library(htl_core STATIC
    src/Config.cpp src/Config.h
    src/ControlService.cpp src/ControlService.h
    src/lifecycle/UnitBackend.h
    src/lifecycle/Sources.h
    src/lifecycle/SystemdBackend.cpp src/lifecycle/SystemdBackend.h
    src/lifecycle/BusNameWatcher.cpp src/lifecycle/BusNameWatcher.h
    src/lifecycle/Fakes.cpp src/lifecycle/Fakes.h
    src/lifecycle/LifecycleController.cpp src/lifecycle/LifecycleController.h
    src/wayland/ToplevelWatcher.cpp src/wayland/ToplevelWatcher.h
    src/input/InputRouter.cpp src/input/InputRouter.h
    src/bluetooth/BluetoothManager.cpp src/bluetooth/BluetoothManager.h
    src/bluetooth/BluezAgent.h
    src/bluetooth/DeviceModel.cpp src/bluetooth/DeviceModel.h
    src/status/NetworkStatus.cpp src/status/NetworkStatus.h
    src/network/NetworkSettings.cpp src/network/NetworkSettings.h
    src/network/WifiModel.cpp src/network/WifiModel.h
    src/idle/IdleMonitor.cpp src/idle/IdleMonitor.h
    src/dbus/DBusTypes.cpp src/dbus/DBusTypes.h
    src/status/Clock.cpp src/status/Clock.h
    src/ui/IconProvider.cpp src/ui/IconProvider.h
    src/ui/WindowController.cpp src/ui/WindowController.h
    src/ui/OverlayState.cpp src/ui/OverlayState.h
    "${PROTO_HEADER}" "${PROTO_CODE}"
)
target_include_directories(htl_core PUBLIC src "${PROTO_DIR}")
target_link_libraries(htl_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::DBus Qt6::Network Qt6::Svg
    LayerShellQtInterface
    PkgConfig::WAYLAND
    PkgConfig::LIBEVDEV
)

qt_add_executable(home-theatre-launcher src/main.cpp)
target_compile_definitions(home-theatre-launcher PRIVATE HTL_VERSION="${PROJECT_VERSION}")
target_link_libraries(home-theatre-launcher PRIVATE htl_core)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

file(GLOB_RECURSE HTL_GLYPHS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" assets/glyphs/*.svg)

qt_add_qml_module(home-theatre-launcher
    URI HomeTheatre
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Label.qml
        qml/Icon.qml
        qml/FocusFrame.qml
        qml/FlatButton.qml
        qml/IndeterminateBar.qml
        qml/Separator.qml
        qml/Glyph.qml
        qml/HintBar.qml
        qml/AppTile.qml
        qml/StatusRow.qml
        qml/HomeView.qml
        qml/TransitionView.qml
        qml/ErrorView.qml
        qml/Toggle.qml
        qml/CheckBox.qml
        qml/PanelItem.qml
        qml/DeviceRow.qml
        qml/BluetoothView.qml
        qml/PairingDialog.qml
        qml/DigitEntry.qml
        qml/NoAdapterView.qml
        qml/FatalConfigView.qml
        qml/StatusIcons.qml
        qml/NetworkView.qml
        qml/NetworkRow.qml
        qml/EthernetDetails.qml
        qml/Segmented.qml
        qml/TextEntry.qml
        qml/KeyButton.qml
        qml/IdleView.qml
        qml/Slideshow.qml
        qml/IdleVideo.qml
        qml/OverlayView.qml
        qml/OverlayButton.qml
)

# Icons live at :/icons, :/apps, :/backgrounds and :/glyphs.
foreach(dir icons apps backgrounds)
    file(GLOB files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets/${dir}" "assets/${dir}/*.svg")
    set(full "")
    foreach(f ${files})
        list(APPEND full "assets/${dir}/${f}")
        set_source_files_properties("assets/${dir}/${f}" PROPERTIES QT_RESOURCE_ALIAS "${f}")
    endforeach()
    qt_add_resources(home-theatre-launcher "htl_${dir}" PREFIX "/${dir}" FILES ${full})
endforeach()
set(glyph_files "")
foreach(f ${HTL_GLYPHS})
    string(REPLACE "assets/glyphs/" "" alias "${f}")
    set_source_files_properties("${f}" PROPERTIES QT_RESOURCE_ALIAS "${alias}")
    list(APPEND glyph_files "${f}")
endforeach()
qt_add_resources(home-theatre-launcher "htl_glyphs" PREFIX "/glyphs" FILES ${glyph_files})

install(TARGETS home-theatre-launcher RUNTIME DESTINATION bin)

if(HTL_BUILD_TESTS)
    enable_testing()
    foreach(t tst_config tst_lifecycle tst_input tst_network tst_idle)
        qt_add_executable(${t} tests/${t}.cpp)
        target_link_libraries(${t} PRIVATE htl_core Qt6::Test)
        add_test(NAME ${t} COMMAND ${t})
        set_tests_properties(${t} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
endif()
