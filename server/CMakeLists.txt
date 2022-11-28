add_executable(flash-server
    main.c
    database.c
    database.h
    server.h
)
target_link_libraries(flash-server common)

# SQLite
include(FindSQLite3)
include_directories(${SQLite3_INCLUDE_DIRS})
target_link_libraries(flash-server ${SQLite3_LIBRARIES})

# Pour embarquer le fichier "init_db.sql" dans le binaire
add_library(init_db_sql init_db_sql.o)
set_source_files_properties(init_db_sql.o PROPERTIES EXTERNAL_OBJECT true GENERATED true)
set_target_properties(init_db_sql PROPERTIES LINKER_LANGUAGE C)
add_custom_command(
    OUTPUT init_db_sql.o
    COMMAND ld -r -b binary -o ${CMAKE_CURRENT_BINARY_DIR}/init_db_sql.o init_db.sql
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/init_db.sql
)
target_link_libraries(flash-server init_db_sql)
