project(C)
cmake_minimum_required(VERSION 2.6)

include_directories(common)

add_library(common common/codec.c)

add_executable(flash-server server/main.c server/server.h)
target_link_libraries(flash-server common)

add_executable(flash-client client/main.c)
target_link_libraries(flash-client common)
