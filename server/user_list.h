#ifndef _USER_LIST_H_
#define _USER_LIST_H_

#include <stdbool.h>
#include <stddef.h>

#include "codec.h"

/**
 * Linked relational table of username <-> fd <-> receive buffer
 */
typedef struct user_list_node_s {
    int fd;
    user_name user_name;
    char frame_receive_buffer[IO_BUFFER_SIZE];
    size_t frame_receive_buffer_len;

    struct user_list_node_s* next;
} user_list_node;

/**
 * Inserts a file descriptor in the list and returns its associated new node
 *
 * Behavior is undefined if the descriptor is already present
 */
user_list_node* user_list_node_insert(user_list_node** list, int fd);

user_list_node* user_list_node_find(user_list_node* list, int fd);

user_list_node* user_list_node_find_by_name(user_list_node* list, const user_name name);

/**
 * Tries to remove a node from its file descriptor in the list. Returns true if the removal is successful.
 */
bool user_list_node_delete(user_list_node** list, int fd);

#endif
