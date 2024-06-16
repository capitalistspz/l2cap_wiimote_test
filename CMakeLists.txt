cmake_minimum_required(VERSION 3.28)
project(l2cap_wiimote_test)

set(CMAKE_CXX_STANDARD 23)

find_package(PkgConfig REQUIRED)

pkg_search_module(bluez IMPORTED_TARGET REQUIRED bluez)

add_executable(l2cap_wiimote_test main.cpp print.h WiimoteDevice.h)


target_link_libraries(l2cap_wiimote_test PRIVATE PkgConfig::bluez)
