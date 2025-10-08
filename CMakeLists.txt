cmake_minimum_required(VERSION 3.21)
project(CameraWall)
set(CMAKE_CXX_STANDARD 17)

# VÁLASSZ EGYET a telepített kitted szerint:

# MinGW build esetén:
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/mingw_64")

# --- vagy ---

# MSVC (Visual Studio) build esetén:
# set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)
qt_standard_project_setup()

add_executable(CameraWall main.cpp)
target_link_libraries(CameraWall PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)
