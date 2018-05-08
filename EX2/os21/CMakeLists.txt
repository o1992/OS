cmake_minimum_required(VERSION 3.8)
project(os21)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES demo_jmp.c uthreads.h demo_singInt_handler.c demo_itimer.c uthreads.cpp Thread.cpp Thread.h test5.cpp)
add_executable(os21 ${SOURCE_FILES})