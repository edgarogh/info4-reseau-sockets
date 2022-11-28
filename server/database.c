#include <assert.h>
#include <ctype.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "database.h"

#define DATABASE_PATH "twiiiiiter.sqlite"

// Le fichier "/server/init_db.sql" est embarqué dans le binaire, linké et accessibles au travers de ces
// deux symboles (c.f. "/server/CMakeLists.txt" où l'étape de build se passe).
extern char _binary_init_db_sql_start[];
extern char _binary_init_db_sql_end;

static sqlite3* db = NULL;
static char* sqlite_error_message;

static bool is_only_whitespace(char* string, const char* end) {
    for (char* c = string; c < end; c++) {
        if (!isspace(*c)) return false;
    }

    return true;
}

static int database_initialize_callback(void* table_count, int column_count, char** row_values, char** columns) {
    assert(column_count == 1);
    *((size_t*) table_count) = strtoul(row_values[0], NULL, 10);
    return 0;
}

void database_initialize() {
    assert(sqlite3_open_v2(DATABASE_PATH, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) == SQLITE_OK);
    printf("[INFO] Using SQLite %s\n", sqlite3_libversion());

    size_t table_count = -1;
    // language=sqlite
    int result = sqlite3_exec(
        db,
        "select count(*) from sqlite_master where type='table'",
        database_initialize_callback,
        &table_count,
        &sqlite_error_message
    );
    assert(result == SQLITE_OK);

    assert(table_count >= 0);
    if (table_count == 0) {
        printf("[INFO] Initializing the database");
        long length = &_binary_init_db_sql_end - &_binary_init_db_sql_start[0];
        for (char* statement = _binary_init_db_sql_start; !is_only_whitespace(statement, &_binary_init_db_sql_end);) {
            sqlite3_stmt* stmt;
            result = sqlite3_prepare_v2(db, statement, (int) length, &stmt, &statement);
            assert(result == SQLITE_OK);
            int step;
            do step = sqlite3_step(stmt); while (step == SQLITE_ROW);
            assert(step == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
        printf(". DONE\n");
        assert(sqlite3_close(db) == SQLITE_OK);
    } else if (table_count == 3) {
        printf("[INFO] Found existing database in " DATABASE_PATH "\n");
    } else {
        // Nombre de tables non conventionnel trouvé
        assert(false);
    }

    // On active les clés étrangères sans quoi SQLite ne les vérifie pas

    // language=sqlite
    result = sqlite3_exec(db, "pragma foreign_keys = on", NULL, NULL, &sqlite_error_message);
    assert(result == SQLITE_OK);
}

followee_iterator database_list_followee(char* follower) {
    sqlite3_stmt* stmt;
    // language=sqlite
    int result = sqlite3_prepare_v2(db, "select followee from followings where follower = ?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, follower, (int) strnlen(follower, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    assert(result == SQLITE_OK);
    return stmt;
}

bool database_list_followee_next(followee_iterator cursor, user_name* out) {
    switch (sqlite3_step(cursor)) {
        case SQLITE_DONE:
            return false;
        case SQLITE_ROW:
            memset(out, 0, MAX_USERNAME_LENGTH);
            memcpy(out, sqlite3_column_text(cursor, 0), MAX_USERNAME_LENGTH);
            return true;
        default:
            assert(false);
    }
}
