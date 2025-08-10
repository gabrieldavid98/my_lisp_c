cmake_minimum_required(VERSION 3.10)

project(my_lisp)

add_subdirectory(mpc)

add_executable(my_lisp main.c)

target_link_libraries(my_lisp mpclib)