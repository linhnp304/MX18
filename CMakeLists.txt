cmake_minimum_required(VERSION 3.19)

project(MX18 VERSION 1.0.926 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Đích tối thiểu là Qt 6.4 (bản đi kèm Ubuntu 24.04); máy dev có thể mới hơn.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network)

# MSVC mặc định đọc file nguồn theo code page hệ thống -> mã tiếng Việt trong
# chuỗi và chú thích sẽ hỏng nếu không ép UTF-8.
if(MSVC)
    add_compile_options(/utf-8 /W3)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

set(MX18_SOURCES
    src/main.cpp

    src/core/AppPaths.cpp
    src/core/AppPaths.h
    src/core/GeoCalc.cpp
    src/core/GeoCalc.h
    src/core/JsonFile.cpp
    src/core/JsonFile.h
    src/core/Settings.cpp
    src/core/Settings.h

    src/map/DbfReader.cpp
    src/map/DbfReader.h
    src/map/ShpReader.cpp
    src/map/ShpReader.h
    src/map/Projection.cpp
    src/map/Projection.h
    src/map/MapData.cpp
    src/map/MapData.h
    src/map/MapPalette.cpp
    src/map/MapPalette.h

    src/net/PingService.cpp
    src/net/PingService.h
    src/net/LinkConfig.cpp
    src/net/LinkConfig.h
    src/net/DataLink.cpp
    src/net/DataLink.h
    src/net/RawIqStore.cpp
    src/net/RawIqStore.h

    src/proto/Dataframe.cpp
    src/proto/Dataframe.h
    src/proto/Packets.cpp
    src/proto/Packets.h
    src/proto/RawIq.cpp
    src/proto/RawIq.h

    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/IconFactory.cpp
    src/ui/IconFactory.h
    src/ui/SplashWindow.cpp
    src/ui/SplashWindow.h
    src/ui/MapView.cpp
    src/ui/MapView.h
    src/ui/AmplitudeView.cpp
    src/ui/AmplitudeView.h
    src/ui/ControlPanel.cpp
    src/ui/ControlPanel.h
    src/ui/SettingsTab.cpp
    src/ui/SettingsTab.h
    src/ui/ControlTab.cpp
    src/ui/ControlTab.h
    src/ui/ControlWidgets.cpp
    src/ui/ControlWidgets.h
    src/ui/AdminTabs.cpp
    src/ui/AdminTabs.h
    src/ui/MhStatusPopup.cpp
    src/ui/MhStatusPopup.h
    src/ui/EngineerWindow.cpp
    src/ui/EngineerWindow.h
    src/ui/FlowLayout.cpp
    src/ui/FlowLayout.h
    src/ui/IqPlots.cpp
    src/ui/IqPlots.h
    src/ui/ViewIqWindow.cpp
    src/ui/ViewIqWindow.h
    src/ui/StatusPanel.cpp
    src/ui/StatusPanel.h
    src/ui/SlidePopup.cpp
    src/ui/SlidePopup.h
    src/ui/Popups.cpp
    src/ui/Popups.h
    src/ui/SetupDialogs.cpp
    src/ui/SetupDialogs.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
)

qt_add_executable(MX18 WIN32 ${MX18_SOURCES})

target_include_directories(MX18 PRIVATE src)
target_link_libraries(MX18 PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

set_target_properties(MX18 PROPERTIES
    OUTPUT_NAME MX18
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE OFF
)

install(TARGETS MX18 RUNTIME DESTINATION .)
