cmake_minimum_required(VERSION 3.22)
project(CameraWall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(CameraWall WIN32
    src/main.cpp
    src/camerawall.h
    src/camerawall.cpp
    src/videotile.h
    src/videotile.cpp
    src/editcameradialog.h
    src/editcameradialog.cpp
    src/onvifclient.h
    src/onvifclient.cpp
    src/util.h
    src/util.cpp
    src/language.h
    src/language.cpp
    src/reorderdialog.h
    src/reorderdialog.cpp
)

qt_add_resources(CameraWall lang_res
    PREFIX "/"
    FILES
        lang/hu.json
        lang/en.json
)

qt_add_resources(CameraWall icons_res
    PREFIX "/icons"
    FILES
        res/app.ico         # Windows/Qt-hoz jó
        res/app_256.png
)

if (WIN32)
    set(APP_ICON_RC ${CMAKE_CURRENT_SOURCE_DIR}/res/appicon.rc)
    # Az RC-t hozzá kell adni az exe forrásaihoz:
    target_sources(CameraWall PRIVATE ${APP_ICON_RC})
endif()

target_link_libraries(CameraWall PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
)

target_include_directories(CameraWall PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
