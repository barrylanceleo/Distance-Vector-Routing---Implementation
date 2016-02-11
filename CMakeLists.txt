cmake_minimum_required(VERSION 3.3)
project(Distance_Vector_Routing_Protocol___Implementation)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(SOURCE_FILES main.c Server.c Server.h main.h SocketUtils.c SocketUtils.h list.c list.h user_interface.c user_interface.h)
add_executable(Distance_Vector_Routing_Protocol___Implementation ${SOURCE_FILES})
