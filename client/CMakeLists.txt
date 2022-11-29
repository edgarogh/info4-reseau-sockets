set(EXE_NAME ${CMAKE_PROJECT_NAME}-client)

add_executable(${EXE_NAME} main.c)
target_link_libraries(${EXE_NAME} common)
