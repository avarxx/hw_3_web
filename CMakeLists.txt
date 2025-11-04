cmake_minimum_required(VERSION 3.16)
project(P2P_Network_App)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

include_directories(src)

set(SOURCES
    src/main.cpp
    src/rendezvous/rendezvous_server.cpp
    src/p2p/p2p_client.cpp
)

add_executable(p2p_app ${SOURCES})

set_target_properties(p2p_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

