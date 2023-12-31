cmake_minimum_required(VERSION 3.25)
project(PW2 C)

set(CMAKE_C_STANDARD 11)

add_executable(PW2
        channel.h
        channel.c
        mimpi_common.h
        mimpirun.c
        mimpi_common.c
        mimpi.h
        mimpi.c
        examples/hello.c
        moja.h
        main.c
)
