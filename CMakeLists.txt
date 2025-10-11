cmake_minimum_required(VERSION 3.22)
project(CameraWall LANGUAGES CXX)

# --- Put outputs into build/release for all configs ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CameraWall")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CameraWall")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/CameraWall")

# Multi-config generátorokhoz (pl. Visual Studio, Ninja Multi-Config) is fixáld:
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOUPPER ${cfg} CFG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} "${CMAKE_BINARY_DIR}/CameraWall")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG} "${CMAKE_BINARY_DIR}/CameraWall")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG} "${CMAKE_BINARY_DIR}/CameraWall")
endforeach()

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

# --- Minimal, robust windeployqt hívás (DLL-ek az EXE mellé) ---
if (WIN32)
  set(WINDEPLOYQT_EXE "c:/Qt/6.9.3/mingw_64/bin/windeployqt.exe")

  # (nem kötelező) írd ki, hogy biztosan jó helyen keressük:
  message(STATUS "windeployqt path: ${WINDEPLOYQT_EXE}")

  if (EXISTS "${WINDEPLOYQT_EXE}")
    add_custom_command(TARGET CameraWall POST_BUILD
      # FONTOS: az EXE legyen az első paraméter!
      COMMAND "${WINDEPLOYQT_EXE}" 
              --release
              --multimedia
              --network
              --verbose 2
              "$<TARGET_FILE:CameraWall>"
      # (ha szeretnéd, ide betehető még: --compiler-runtime)
      COMMENT "Running windeployqt for $<TARGET_FILE:CameraWall>"
      VERBATIM
    )
  else()
    message(WARNING "windeployqt not found at: ${WINDEPLOYQT_EXE} — skipping Qt DLL deployment.")
  endif()
endif()

set(RUNTIME_DIR "${CMAKE_BINARY_DIR}/CameraWall")

set_target_properties(CameraWall PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${RUNTIME_DIR}"
)
# --- background.png másolása az exe mellé ---
add_custom_command(TARGET CameraWall POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RUNTIME_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/res/background.png"
            "${RUNTIME_DIR}/background.png"
    COMMENT "Copy default background.png next to the executable"
)