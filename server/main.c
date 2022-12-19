#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include "constants.h"
#include "database.h"
#include "server.h"

// Maximum d'évènements retournés par epoll lors d'un appel système
#define EPOLL_MAX_EVENTS 16

int main(int argc, char** argv) {
    assert(argc <= 2);
    uint16_t port;
    if (argc == 2) {
        port = strtoul(argv[1], NULL, 10);
    } else {
        port = 7878;
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_socket != 0);

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = {
            .s_addr = INADDR_ANY,
        },
        .sin_port = htons(port),
        .sin_zero = { 0 },
    };

    assert(bind(server_socket, (struct sockaddr*) &address, sizeof(address)) >= 0);
    assert(listen(server_socket, 16) == 0);

    database_initialize();

    int epoll = epoll_create1(0);
    assert (epoll > 0);
    // EPOLLIN sur un socket d'écoute correspond à une connexion entrante
    struct epoll_event server_socket_epollin = { .events = EPOLLIN, .data.fd = server_socket };
    epoll_ctl(epoll, EPOLL_CTL_ADD, server_socket, &server_socket_epollin);

    server_state server = {
        .server_socket = server_socket,
        .epoll = epoll,
        .users = NULL,
    };

    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (true) {
        int remaining_events;
        do {
            remaining_events = epoll_wait(epoll, events, EPOLL_MAX_EVENTS, -1);
        } while (remaining_events == -1 && errno == EINTR); // Obligatoire pour que GDB fonctionne
        assert(remaining_events > 0);

        for (int i = 0; i < remaining_events; i++) {
            struct epoll_event* event = &events[i];
            handle_event(&server, event);
        }
    }
}

#define SUCCESS_OR_RETURN(value, args...) if (value < 0) { printf("[WARNING] " args); return; }

void handle_event(server_state* server, struct epoll_event* event) {
    if (event->data.fd == server->server_socket) { // Nouvelle connexion
        if (event->events & EPOLLHUP || event->events & EPOLLERR) {
            close(server->server_socket);
            exit(1);
        }

        int fd = event->data.fd;
        struct sockaddr address;
        unsigned int addrlen = sizeof(address);
        int sock = accept(fd, &address, &addrlen);
        SUCCESS_OR_RETURN(sock, "Couldn't accept a client: errno %d\n", errno);

        // rend le nouveau socket non bloquant
        // et l'ajoute au contexte epoll
        int flags = fcntl(fd, F_GETFL, NULL);
        SUCCESS_OR_RETURN(flags, "Couldn't make incoming connection non-blocking: errno %d\n", errno);
        SUCCESS_OR_RETURN(fcntl(fd, F_SETFL, flags | O_NONBLOCK), "fnctl SET failed: errno %d\n", errno);

        struct epoll_event socket_epollin = { .events = EPOLLIN, .data.fd = sock };
        SUCCESS_OR_RETURN(
            epoll_ctl(server->epoll, EPOLL_CTL_ADD, sock, &socket_epollin),
            "Couldn't add incoming connection to the epoll pool: errno %d\n", errno
        );

        printf("[INFO] %d is joining\n", sock);
        user_list_node_insert(&server->users, sock);
    } else if (event->events & EPOLLIN) { // Probablement un nouveau message sur un socket connecté à un client
        int fd = event->data.fd;
        user_list_node* user = user_list_node_find(server->users, fd);
        if (user == NULL) {
            printf("[ERROR] Can't find file descriptor %d in the connected player list. Kicking them.\n", fd);
            kick_user(server, fd);
            return;
        }

        size_t already_read = user->frame_receive_buffer_len;
        ssize_t bytes_read = read(fd, user->frame_receive_buffer + already_read, IO_BUFFER_SIZE - already_read);
        if (bytes_read > 0) {
            user->frame_receive_buffer_len += bytes_read;
            if (user->frame_receive_buffer_len == IO_BUFFER_SIZE) {
                user->frame_receive_buffer_len = 0;
                message_c2s message;
                if (decode_c2s(user->frame_receive_buffer, &message)) {
                    process_message(server, user, &message);
                } else {
                    printf("[WARNING] Invalid message sent by client %d\n", fd);
                }
            }
        } else {
            // EOF? => disconnect socket
            kick_user(server, fd);
        }
    } else {
        printf("[ERROR] Unhandled epoll event %d, ignoring\n", event->events);
    }
}

void process_message(server_state* server, user_list_node* user, const message_c2s* message) {
    int fd = user->fd;
    char* username = user->user_name;

    if (message->tag == MESSAGE_C2S_JOIN_AS) {
        if (username[0] != 0) {
            printf("[WARNING] User %.*s is trying to rename themselves, which is forbidden\n", MAX_USERNAME_LENGTH, username);
            send_message_immediately(fd, (message_s2c) {
                    .tag = MESSAGE_S2C_KICK,
                    .kick = KICK_REASON_PROTOCOL_ERROR,
            });
            kick_user(server, fd);
            return;
        } else {
            if (message->join_as[0] == 0) {
                // Name can't be empty
                send_message_immediately(fd, (message_s2c) {
                    .tag = MESSAGE_S2C_LOGIN_STATUS,
                    .login_status = LOGIN_STATUS_ILLEGAL_NAME,
                });
                return;
            } else if (user_list_node_find_by_name(server->users, message->join_as)) {
                // Name can't be already online
                send_message_immediately(fd, (message_s2c) {
                    .tag = MESSAGE_S2C_LOGIN_STATUS,
                    .login_status = LOGIN_STATUS_ALREADY_USED,
                });
                return;
            } else {
                // Attach the username to the current user node
                strncpy(username, message->join_as, MAX_USERNAME_LENGTH);
                database_update_user(username, true);
                send_message_immediately(fd, (message_s2c) {
                    .tag = MESSAGE_S2C_LOGIN_STATUS,
                    .login_status = LOGIN_STATUS_OK,
                });
            }
        }
    }

    switch (message->tag) {
        case MESSAGE_C2S_JOIN_AS:;
            twiiiiit_iterator missed_it = database_list_missed_twiiiiits(username);
            database_twiiiiit twiiiiit;
            while (database_twiiiiits_next(missed_it, &twiiiiit)) {
                message_s2c twiiiiit_msg = (message_s2c) {
                    .tag = MESSAGE_S2C_RECEIVED_MESSAGE,
                    .received_message.date = twiiiiit.date,
                };
                strncpy(twiiiiit_msg.received_message.author, twiiiiit.author, MAX_USERNAME_LENGTH);
                strncpy(twiiiiit_msg.received_message.message, twiiiiit.message, MESSAGE_MAX_LENGTH);
                send_message_immediately(fd, twiiiiit_msg);
            }
            return;
        case MESSAGE_C2S_SUBSCRIBE_TO:;
            enum subscribe_result result = database_follow(username, message->subscribe_to);
            send_message_immediately(fd, (message_s2c) {
                .tag = MESSAGE_S2C_SUBSCRIBE_RESULT,
                .subscribe_result = result,
            });
            return;
        case MESSAGE_C2S_UNSUBSCRIBE_TO:;
            enum subscribe_result u_result = database_unfollow(username, message->unsubscribe_to);
            send_message_immediately(fd, (message_s2c) {
                .tag = MESSAGE_S2C_SUBSCRIBE_RESULT,
                .subscribe_result = u_result,
            });
            return;
        case MESSAGE_C2S_LIST_SUBSCRIPTIONS:;
            followee_iterator it = database_list_followee(username);
            message_s2c subscription_entry = {
                .tag = MESSAGE_S2C_SUBSCRIPTION_ENTRY,
            };

            while (database_list_followee_next(it, subscription_entry.subscription_entry)) {
                send_message_immediately(fd, subscription_entry);
            }

            // We finish the list with a blank entry
            memset(subscription_entry.subscription_entry, 0, MAX_USERNAME_LENGTH);
            send_message_immediately(fd, subscription_entry);
            return;
        case MESSAGE_C2S_PUBLISH:;
            time_t twiiiiit_date = database_save_twiiiiit(username, message->publish);
            message_s2c twiiiiit_msg = (message_s2c) {
                .tag = MESSAGE_S2C_RECEIVED_MESSAGE,
                .received_message.date = twiiiiit_date,
            };
            strncpy(twiiiiit_msg.received_message.author, username, MAX_USERNAME_LENGTH);
            strncpy(twiiiiit_msg.received_message.message, message->publish, MESSAGE_MAX_LENGTH);
            // Broadcast twiiiiit
            for (user_list_node* u = server->users; u; u = u->next) {
                send_message_immediately(u->fd, twiiiiit_msg);
            }
            return;
    }
}

/**
 * In the future, we may want to send data using epoll too
 */
bool send_message_immediately(int fd, message_s2c message) {
    char frame[IO_BUFFER_SIZE];
    memset(frame, 0, IO_BUFFER_SIZE);
    encode_s2c(&message, frame);

    ssize_t remaining = IO_BUFFER_SIZE;
    while (remaining > 0) {
        ssize_t written = write(fd, frame + (IO_BUFFER_SIZE - remaining), remaining);
        if (written < 0) {
            return false;
        }
        remaining -= written;
    }

    return true;
}

void kick_user(server_state* server, int user_fd) {
    // TODO database_update_player
    printf("[INFO] %d is leaving\n", user_fd);
    user_list_node_delete(&server->users, user_fd);
    epoll_ctl(server->epoll, EPOLL_CTL_DEL, user_fd, NULL);
    close(user_fd);
}
