cmake_minimum_required(VERSION 3.10)
project(PROGETTO_SISTEMI_OPERATIVI_2025 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/server
)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(collector_client
    client/main_client.c
    client/client.c
)

add_executable(hash_server
    server/main_server.c
    server/server.c
)

target_link_libraries(hash_server
    OpenSSL::Crypto
    Threads::Threads
)

target_link_libraries(collector_client
    Threads::Threads
)