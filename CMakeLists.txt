cmake_minimum_required(VERSION 3.21)

project(NuClip VERSION 0.1.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 QuickDialogs2 Multimedia Concurrent Svg Network)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

set(NUCLIP_SOURCES
    src/main.cpp
    src/app/SelfTest.cpp
    src/core/Log.cpp
    src/core/Paths.cpp
    src/core/Settings.cpp
    src/core/Theme.cpp
    src/platform/WinUtil.cpp
    src/platform/HotkeyManager.cpp
    src/platform/DeviceCatalog.cpp
    src/platform/AppCatalog.cpp
    src/platform/GameDetector.cpp
    src/platform/TrayController.cpp
    src/platform/OverlayService.cpp
    src/platform/DiscordPresence.cpp
    src/audio/WasapiInput.cpp
    src/recording/FfmpegLocator.cpp
    src/recording/FfmpegJob.cpp
    src/recording/CaptureCommand.cpp
    src/recording/RecordingEngine.cpp
    src/library/ClipRepository.cpp
    src/library/ClipFilterModel.cpp
    src/editor/EditorSession.cpp
    src/export/Exporter.cpp
    src/storage/StorageMonitor.cpp
    src/update/UpdateChecker.cpp
)

qt_add_executable(NuClip WIN32 ${NUCLIP_SOURCES} resources/app.rc)

qt_add_qml_module(NuClip
    URI NuClip
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/NcIcon.qml
        qml/components/NcButton.qml
        qml/components/NcToggle.qml
        qml/components/NcSlider.qml
        qml/components/NcCombo.qml
        qml/components/NcTextField.qml
        qml/components/NcCard.qml
        qml/components/NcCallout.qml
        qml/components/NcSection.qml
        qml/components/LevelMeter.qml
        qml/components/Timeline.qml
        qml/components/ClipMenu.qml
        qml/pages/HomePage.qml
        qml/pages/ClipsPage.qml
        qml/pages/EditorPage.qml
        qml/pages/RecordingPage.qml
        qml/pages/SettingsPage.qml
        qml/overlay/CaptureToast.qml
        qml/overlay/CaptureCard.qml
)

qt_add_resources(NuClip "assets"
    PREFIX "/assets"
    FILES resources/nuclip.svg resources/saved.mp3
)

target_include_directories(NuClip PRIVATE src)
target_compile_definitions(NuClip PRIVATE
    NUCLIP_VERSION="${PROJECT_VERSION}"
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(NuClip PRIVATE /W4 /permissive- /utf-8 /EHsc /external:W0 /external:anglebrackets)
endif()

target_link_libraries(NuClip PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Network
    d3d11
    dxgi
    ole32
    uuid
    mmdevapi
    propsys
    version
    user32
    shell32
    advapi32
    dwmapi
)

set(NUCLIP_FFMPEG "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/ffmpeg.exe" CACHE FILEPATH "FFmpeg executable")
set(NUCLIP_FFPROBE "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/ffprobe.exe" CACHE FILEPATH "FFprobe executable")
if(NOT EXISTS "${NUCLIP_FFMPEG}" OR NOT EXISTS "${NUCLIP_FFPROBE}")
    message(FATAL_ERROR "Place ffmpeg.exe and ffprobe.exe in third_party/ffmpeg. See README.")
endif()

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin" REQUIRED)

add_custom_command(TARGET NuClip POST_BUILD
    COMMAND "${WINDEPLOYQT}" "$<IF:$<CONFIG:Debug>,--debug,--release>" --no-translations
            --qmldir "${CMAKE_SOURCE_DIR}/qml" "$<TARGET_FILE:NuClip>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NUCLIP_FFMPEG}" "$<TARGET_FILE_DIR:NuClip>/ffmpeg.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NUCLIP_FFPROBE}" "$<TARGET_FILE_DIR:NuClip>/ffprobe.exe"
    COMMENT "Deploying Qt runtime and FFmpeg"
)

install(TARGETS NuClip RUNTIME DESTINATION .)
add_executable(NuClipSetup WIN32 src/setup/Setup.cpp src/setup/Setup.rc)
target_include_directories(NuClipSetup PRIVATE src)
target_compile_definitions(NuClipSetup PRIVATE NOMINMAX UNICODE _UNICODE)
if(MSVC)
    target_compile_options(NuClipSetup PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_options(NuClipSetup PRIVATE "/MANIFEST:NO")
endif()
target_link_libraries(NuClipSetup PRIVATE winhttp bcrypt shell32 ole32 uuid user32 gdi32 advapi32)

