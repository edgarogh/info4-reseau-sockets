#include <assert.h>
#include <errno.h>
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
    } else if (event->events & EPOLLIN) { // Probablement un nouveau message sur un socket connecté à un client
        // TODO remove the following lines, this is temporary
        int fd = event->data.fd;
        char buffer[IO_BUFFER_SIZE];
        ssize_t bytes_read = read(fd, buffer, IO_BUFFER_SIZE);
        if (bytes_read == 0) {
            epoll_ctl(server->epoll, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            return;
        }
        printf("[DEBUG] %ld bytes were read\n", bytes_read);
    } else {
        printf("[ERROR] Unhandled epoll event %d, ignoring\n", event->events);
    }
}
