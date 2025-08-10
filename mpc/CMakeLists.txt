cmake_minimum_required(VERSION 3.10)

add_library(mpclib mpc.c)

target_include_directories(mpclib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

