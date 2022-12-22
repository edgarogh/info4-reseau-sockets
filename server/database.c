#include <assert.h>
#include <ctype.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "constants.h"
#include "database.h"

#define DATABASE_PATH "twiiiiiter.sqlite"

// Le fichier "/server/init_db.sql" est embarqué dans le binaire, linké et accessibles au travers de ces
// deux symboles (c.f. "/server/CMakeLists.txt" où l'étape de build se passe).
extern const char _binary_init_db_sql_start[];
extern const char _binary_init_db_sql_end;

static sqlite3* db = NULL;
static char* sqlite_error_message;

static bool is_only_whitespace(const char* string, const char* end) {
    for (const char* c = string; c < end; c++) {
        if (!isspace(*c)) return false;
    }

    return true;
}

int sqlite3_step_all(sqlite3_stmt* stmt) {
    int step;
    do step = sqlite3_step(stmt); while (step == SQLITE_ROW);
    return step;
}

int64_t ts_now() {
    struct timespec tv;
    timespec_get(&tv,TIME_UTC);
    return tv.tv_sec * (int64_t) 1000000 + tv.tv_nsec / 1000;
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
        for (const char* statement = _binary_init_db_sql_start; !is_only_whitespace(statement, &_binary_init_db_sql_end);) {
            sqlite3_stmt* stmt;
            result = sqlite3_prepare_v2(db, statement, (int) length, &stmt, &statement);
            assert(result == SQLITE_OK);
            assert(sqlite3_step_all(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
        printf(". DONE\n");
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

void database_update_user(const char* user, bool is_online) {
    // language=sqlite
    char* sql = "insert or ignore into users values (?1, ?2) on conflict (name) do update set last_online = ?2";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, user, (int) strnlen(user, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, ts_now());
    assert(sqlite3_step_all(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
}

/**
 * L'abonnement et le désabonnement se font de la même manière (à l'exception du code SQL), et sont donc effectués dans
 * cette fonction. Les points d'entrée publics sont les deux fonctions sous celle-ci.
 *
 * @see database_follow()
 * @see database_unfollow()
 */
static enum subscribe_result database_follow_unfollow(const char* follower, const char* followee, const char* sql) {
    int follower_len = (int) strnlen(follower, MAX_USERNAME_LENGTH);
    int followee_len = (int) strnlen(followee, MAX_USERNAME_LENGTH);

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, follower, follower_len, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, followee, followee_len, SQLITE_STATIC);

    switch (sqlite3_step_all(stmt)) {
        case SQLITE_DONE:
            sqlite3_finalize(stmt);
            break;
        case SQLITE_CONSTRAINT:
            sqlite3_finalize(stmt);
            int ext_err = sqlite3_extended_errcode(db);
            if (ext_err == SQLITE_CONSTRAINT_FOREIGNKEY) return SUBSCRIBE_RESULT_NOT_FOUND;
            if (ext_err == SQLITE_CONSTRAINT_UNIQUE) return SUBSCRIBE_RESULT_UNCHANGED;
            assert(false);
        default:
            assert(false);
    }

    // S'il y a 0 changements, cela signifie qu'aucune ligne n'a été supprimée et que donc l'abonnement n'existait
    // pas de base.
    return sqlite3_changes(db) != 0 ? SUBSCRIBE_RESULT_OK : SUBSCRIBE_RESULT_UNCHANGED;
}

enum subscribe_result database_follow(const char* follower, const char* followee) {
    // On ne peut pas s'abonner à soi-même.
    // Le message d'erreur n'est pas des plus descriptifs, mais c'est quelque chose que le client aurait pu détecter.
    if (strncmp(follower, followee, MAX_USERNAME_LENGTH) == 0) return SUBSCRIBE_RESULT_NOT_FOUND;

    // language=sqlite
    char* sql = "insert into followings values (?, ?)";
    return database_follow_unfollow(follower, followee, sql);
}

enum subscribe_result database_unfollow(const char* follower, const char* followee) {
    // language=sqlite
    char* sql = "delete from followings where follower = ? and followee = ?";
    return database_follow_unfollow(follower, followee, sql);
}

user_iterator database_list_followee(const char* follower) {
    sqlite3_stmt* stmt;
    // language=sqlite
    int result = sqlite3_prepare_v2(db, "select followee from followings where follower = ?", -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, follower, (int) strnlen(follower, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    return stmt;
}

user_iterator database_list_followers(const char* followee) {
    sqlite3_stmt* stmt;
    // language=sqlite
    int result = sqlite3_prepare_v2(db, "select follower from followings where followee = ?", -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, followee, (int) strnlen(followee, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    return stmt;
}

bool database_users_next(user_iterator restrict cursor, char* restrict out) {
    switch (sqlite3_step(cursor)) {
        case SQLITE_DONE:
            sqlite3_finalize(cursor);
            return false;
        case SQLITE_ROW:
            assert(sqlite3_column_count(cursor) == 1);
            memset(out, 0, MAX_USERNAME_LENGTH);
            strncpy(out, (char*) sqlite3_column_text(cursor, 0), MAX_USERNAME_LENGTH);
            return true;
        default:
            assert(false);
    }
}

time_t database_save_twiiiiit(const char* author, const char* message) {
    // language=sqlite
    char* sql = "insert into twiiiiits values (?, ?, ?)";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    time_t now = ts_now();
    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_text(stmt, 2, author, (int) strnlen(author, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message, (int) strnlen(message, MESSAGE_MAX_LENGTH), SQLITE_STATIC);
    assert(sqlite3_step_all(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return now;
}

twiiiiit_iterator database_list_missed_twiiiiits(const char* follower) {
    // C'est effrayant, mais ça permet de tout récupérer en une requête
    // language=sqlite
    char* sql = "select t.* from twiiiiits t inner join followings f on t.author = f.followee inner join users r on f.follower = r.name where f.follower = ? and t.date >= r.last_online order by t.date";

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    assert(result == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, follower, (int) strnlen(follower, MAX_USERNAME_LENGTH), SQLITE_STATIC);
    return stmt;
}

bool database_twiiiiits_next(twiiiiit_iterator restrict iterator, database_twiiiiit* restrict out) {
    switch (sqlite3_step(iterator)) {
        case SQLITE_DONE:
            sqlite3_finalize(iterator);
            return false;
        case SQLITE_ROW:
            assert(sqlite3_column_count(iterator) == 3);
            memset(out, 0, sizeof(database_twiiiiit));
            out->date = sqlite3_column_int64(iterator, 0);
            strncpy(out->author, (char*) sqlite3_column_text(iterator, 1), MAX_USERNAME_LENGTH);
            strncpy(out->message, (char*) sqlite3_column_text(iterator, 2), MESSAGE_MAX_LENGTH);
            return true;
        default:
            assert(false);
    }
}
