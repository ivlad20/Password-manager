cmake_minimum_required(VERSION 3.20)
project(PasswordManager)

set(CMAKE_CXX_STANDARD 17)

find_package(unofficial-sodium CONFIG REQUIRED)

add_executable(PasswordManager src/main.cpp)
target_link_libraries(PasswordManager PRIVATE unofficial-sodium::sodium)