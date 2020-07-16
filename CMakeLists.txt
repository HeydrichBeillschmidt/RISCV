cmake_minimum_required(VERSION 3.10.2)
project(RV32I)

set(CMAKE_CXX_STANDARD 14)

add_executable(code
        code.cpp
        pcpu.h
        )
