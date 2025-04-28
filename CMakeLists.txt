cmake_minimum_required(VERSION 3.28)
project(CourseWorkAvtomata)

set(CMAKE_CXX_STANDARD 17)

add_executable(CourseWorkAvtomata main.cpp
        src/codegen.cpp
        src/parser.cpp
        src/scanner.cpp)
