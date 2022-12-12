#include <assert.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
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
            .s_addr = INADDR_ANY
        },
        .sin_port = port,
        .sin_zero = { 0 },
    };

    assert(bind(server_socket, (struct sockaddr*) &address, sizeof(address)) >= 0);
    assert(listen(server_socket, 16) == 0);

    database_initialize();

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

void handle_err(int code, const char * msg) {
    if (code < 0) {
        printf("[ERROR] %s\n", msg);
        exit(1);
    }
}

void handle_event(server_state* server, struct epoll_event* event) {
    if (event->data.fd == server->server_socket) { // Nouvelle connexion
        if(event->events & EPOLLHUP || event->events & EPOLLERR) {
            close(server->server_socket);
            exit(1);
        }

        int fd = event->data.fd;
        struct sockaddr address;
        unsigned int addrlen = sizeof(address);
        int sock = accept(fd, &address, &addrlen);
        handle_err(sock, "Accept failed");

        // rend le nouveau socket non bloquant
        // et l'ajoute au contexte epoll
        int flags = fcntl(fd, F_GETFL, NULL);
        handle_err(flags, "fnctl GET failed");
        handle_err(fcntl(fd, F_SETFL, flags | O_NONBLOCK), "fnctl SET failed");

        struct epoll_event socket_epolling = { .events = EPOLLIN };
        epoll_ctl(server->epoll, EPOLL_CTL_ADD, sock, &socket_epolling);
    } else { // Nouveau message sur un socket connecté à un client

    }
}
