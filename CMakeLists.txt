cmake_minimum_required(VERSION 3.16)
project(can_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(can_monitor
    src/main.cpp
    src/connection.cpp
    src/gvret_parser.cpp
    src/frame_store.cpp
    src/display.cpp
)

target_link_libraries(can_monitor PRIVATE ws2_32)
