cmake_minimum_required(VERSION 3.17)
project(tinyfile)

set(CMAKE_CXX_STANDARD 11)

include_directories(include/)

# API Library
add_library(tinyfile
        include/tinyfile/api.h
        src/api/api.c
        include/tinyfile/types.h)
