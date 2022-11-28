#include <assert.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>

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
            .s_addr = INADDR_ANY
        },
        .sin_port = port,
        .sin_zero = { 0 },
    };

    assert(bind(server_socket, (struct sockaddr*) &address, sizeof(address)) >= 0);
    assert(listen(server_socket, 16) == 0);

    struct sockaddr* client_address;
    // accept(server_socket, &client_address, &sizeof(client_address));
    
    int epoll = epoll_create1(0);
    assert (epoll > 0);
    // EPOLLIN sur un socket d'écoute correspond à une connexion entrante
    struct epoll_event server_socket_epolling = { .events = EPOLLIN };
    epoll_ctl(epoll, EPOLL_CTL_ADD, server_socket, &server_socket_epolling);

    server_state server = {
        .server_socket = server_socket,
        .epoll = epoll,
    };

    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (true) {
        int remaining_events = epoll_wait(epoll, events, EPOLL_MAX_EVENTS, -1);
        assert(remaining_events > 0);
        for (int i = 0; i < remaining_events; i++) {
            struct epoll_event* event = &events[i];
            handle_event(&server, event);
        }
    }
}

void handle_event(server_state* server, struct epoll_event* event) {
    // TODO
}
