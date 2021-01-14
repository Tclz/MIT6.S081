# cmake_minimum_required(VERSION <specify CMake version here>)
project(6_S081 C)

set(CMAKE_C_STANDARD 99)

add_executable(6_S081 main.c user/sleep.c user/pingpong.c user/primes.c user/find.c user/xargs.c)