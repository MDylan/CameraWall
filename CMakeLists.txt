cmake_minimum_required(VERSION 3.22)
project(CameraWall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 csomagok
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)

# Automatikus MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(CameraWall
    src/main.cpp
    src/util.h
    src/onvifclient.h
    src/onvifclient.cpp
    src/videotile.h
    src/videotile.cpp
    src/editcameradialog.h
    src/editcameradialog.cpp
    src/camerawall.h
    src/camerawall.cpp
)

target_link_libraries(CameraWall PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)
