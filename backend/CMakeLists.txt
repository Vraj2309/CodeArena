cmake_minimum_required(VERSION 3.15)
project(CodeArenaBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/crow.h")
    message(FATAL_ERROR "Missing backend/crow.h. Download the Crow single-header file from https://github.com/CrowCpp/Crow and place it in the backend folder.")
endif()

add_executable(codearena main.cpp)

if(WIN32)
    target_link_libraries(codearena ws2_32 mswsock)
endif()
