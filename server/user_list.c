#include <stdlib.h>
#include <string.h>

#include "user_list.h"

user_list_node* user_list_node_insert(user_list_node** list, int fd) {
    user_list_node* next = *list;
    user_list_node* new = malloc(sizeof(user_list_node));
    new->fd = fd;
    memset(new->user_name, 0, MAX_USERNAME_LENGTH);
    new->frame_receive_buffer_len = 0;
    new->next = next;
    return *list = new;
}

user_list_node* user_list_node_find(user_list_node* list, int fd) {
    for (user_list_node* node = list; node != NULL; node = node->next) {
        if (node->fd == fd) {
            return node;
        }
    }

    return NULL;
}

user_list_node* user_list_node_find_by_name(user_list_node* list, const user_name name) {
    for (user_list_node* node = list; node != NULL; node = node->next) {
        if (strncmp(node->user_name, name, MAX_USERNAME_LENGTH) == 0) {
            return node;
        }
    }

    return NULL;
}

bool user_list_node_delete(user_list_node** list, int fd) {
    for (user_list_node** node = list; *node != NULL; node = &(*node)->next) {
        if ((*node)->fd == fd) {
            void* to_free = *node;
            *node = (*node)->next;
            free(to_free);
            return true;
        }
    }

    return false;
}
