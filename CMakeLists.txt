cmake_minimum_required(VERSION 3.15)
project(multichat C)

set(CMAKE_C_STANDARD 11)

add_executable(multichat multicast_chat_client.c)