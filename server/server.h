#ifndef _SERVER_H_
#define _SERVER_H_

#include "user_list.h"

typedef struct {
    int server_socket;
    int epoll;
    user_list_node* users;
} server_state;

void handle_event(server_state* server, struct epoll_event* event);
void process_message(server_state* server, user_list_node* user, const message_c2s* message);
void kick_user(server_state* server, int user_fd);

#endif
