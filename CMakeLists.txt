cmake_minimum_required(VERSION 3.30)
project(wallace)
set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKLS REQUIRED IMPORTED_TARGET gtk4-layer-shell-0)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(wallace main.cpp)
target_link_libraries(wallace PRIVATE PkgConfig::GTK4 PkgConfig::GTKLS)
