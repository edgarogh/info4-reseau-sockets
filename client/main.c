#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>

#include "codec.h"
#include "twiit_list.h"

#define EPOLL_MAX_EVENTS 16
#define CMD_BUFFER_SIZE 1000

typedef struct {
    int client_socket;
    int epoll;
    twiiiiit_list* twiiiiit_list;
    char cmd[CMD_BUFFER_SIZE];
    int cmd_offset;
    char send_buffer[IO_BUFFER_SIZE];
    int send_buffer_len;
    char receive_buffer[IO_BUFFER_SIZE];
    int receive_buffer_len;
} client_state;

void handle_event(client_state* client, struct epoll_event* event);
int send_msg(client_state client);
void receive_msg(client_state client, message_s2c msg);
void publish(client_state client);
void subscribe(client_state client, bool unsub);

int main(int argc, char** argv) {
    assert(argc <= 2);
    uint16_t port;

    char* host = argv[1];
    char* colon = strrchr(host, ':');
    if (colon != NULL) {
        *colon = 0;
        port = strtoul(colon + 1, NULL, 10);
    } else {
        port = DEFAULT_PORT;
    }

    struct hostent* ip = gethostbyname(host);
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    assert(client_socket != 0);

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr = {
            .s_addr = *((in_addr_t*) ip->h_addr),
        },
        .sin_port = htons(port),
        .sin_zero = { 0 },
    };

    assert(connect(client_socket, (struct sockaddr*) &address, sizeof(address)) == 0);
    printf("[INFO] Connected\n");

    char user[MAX_USERNAME_LENGTH];
    printf("USER ID:");
    scanf("%s", user);

    message_c2s message = {
        .tag = MESSAGE_C2S_JOIN_AS,
    };

    client_state client = {
            .client_socket = client_socket,
            .twiiiiit_list = twiit_list_new(),
            .cmd_offset = 0,
            .send_buffer_len = 0,
            .receive_buffer_len = 0,
    };

    memcpy(&message.join_as, user, strnlen(user, MESSAGE_MAX_LENGTH));
    memset(&client.send_buffer, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, client.send_buffer);
    while(client.send_buffer_len != IO_BUFFER_SIZE){
        client.send_buffer_len += send(client_socket, client.send_buffer+client.send_buffer_len , IO_BUFFER_SIZE, 0);
    }
    client.send_buffer_len = 0;
    printf("[INFO] Logged in as %s \n", user);

    int epoll = epoll_create1(0);
    assert (epoll > 0);
    // EPOLLIN sur un socket d'écoute correspond à une connexion entrante
    struct epoll_event server_socket_epollin = { .events = EPOLLIN, .data.fd = client_socket };
    epoll_ctl(epoll, EPOLL_CTL_ADD, client_socket, &server_socket_epollin);

    // Ajout du stdin à l'epoll
    struct epoll_event stdin_epollin = { .events = EPOLLIN, .data.fd = STDIN_FILENO };
    epoll_ctl(epoll, EPOLL_CTL_ADD, STDIN_FILENO, &stdin_epollin );

    client.epoll = epoll;

    struct epoll_event events[EPOLL_MAX_EVENTS];
    while (true) {
        int remaining_events;
        do {
            remaining_events = epoll_wait(epoll, events, EPOLL_MAX_EVENTS, -1);
        } while (remaining_events == -1 && errno == EINTR); // Obligatoire pour que GDB fonctionne
        assert(remaining_events > 0);

        for (int i = 0; i < remaining_events; i++) {
            struct epoll_event *event = &events[i];
            handle_event(&client, event);
        }
    }
}

void handle_event(client_state* client, struct epoll_event* event) {
    if (event->events & EPOLLIN) { // Probablement un nouveau message sur un socket connecté à un client
        if (event->data.fd == STDIN_FILENO) {
            int len = read(STDIN_FILENO, client->cmd + client->cmd_offset, CMD_BUFFER_SIZE - client->cmd_offset);
            switch (len) {
                case 0 :
                    printf("[LOGOUT]\n");
                    close(client->client_socket);
                case -1 :
                    printf("[WARNING] Error \n");
                default :
                    if (client->cmd[len -1] == '\n'){ // ENTER pressed
                        client->cmd_offset += len;
                        client->cmd[client->cmd_offset -1] = 0;
                        send_msg(*client);
                    } else client->cmd_offset += len;

            }
        } else if (event->data.fd == client->client_socket) {
            // TODO if client.received... full
            size_t already_read = client->receive_buffer_len;
            ssize_t bytes_read = read(client->client_socket, client->receive_buffer + already_read, IO_BUFFER_SIZE - already_read);
            if (bytes_read > 0) {
                client->receive_buffer_len += bytes_read;
                if (client->receive_buffer_len == IO_BUFFER_SIZE) {
                    client->receive_buffer_len = 0;
                    message_s2c message;
                    if (decode_s2c(client->receive_buffer, &message)) {
                        receive_msg(*client, message);
                    } else {
                        printf("[WARNING] Invalid message sent by server\n");
                    }
                }
            } else {
                close(client->client_socket);
            }
        } else {
            printf("[ERROR] Unhandled\n"); // TODO
        }
    } else {
        printf("[ERROR] Unhandled epoll event %d, ignoring\n", event->events);
    }
}

int send_msg(client_state client) {
    switch (*client.cmd) {
        case 'P' :
            publish(client);
            return 1;
        case 'S':
            subscribe(client, false);
            return 1;
        case 'U':
            subscribe(client, true);
            return 1;
        default :
            return 0;
    }
}

void receive_msg(client_state client, message_s2c msg){
    switch (msg.tag) {
        case MESSAGE_S2C_LOGIN_STATUS:
            printf("[SERVER]");
            switch (msg.login_status) {
                case LOGIN_STATUS_OK :
                    printf("Connected.\n");
                    break;
                case LOGIN_STATUS_ALREADY_USED :
                    printf("Username already used.\n");
                    break;
                case LOGIN_STATUS_ILLEGAL_NAME :
                    printf("Illegal username.\n");
                    break;
            }
            break;

        case MESSAGE_S2C_RECEIVED_MESSAGE:
            twiiiiit_append(client.twiiiiit_list,msg.received_message);
            printf("[TWIIIIIT] @%s - %s\n", msg.received_message.author, msg.received_message.message);
            break;

        case MESSAGE_S2C_SUBSCRIBE_RESULT:
            printf("[SERVER]");
            switch (msg.subscribe_result) {
                case SUBSCRIBE_RESULT_OK :
                    printf("Subscribed.\n");
                    break;
                case SUBSCRIBE_RESULT_NOT_FOUND :
                    printf("User not found.\n");
                    break;
                case SUBSCRIBE_RESULT_UNCHANGED :
                    printf("Already subscribed to this user.\n");
                    break;
            }
            break;
        case MESSAGE_S2C_SUBSCRIPTION_ENTRY:
            printf("[SERVER] You're subscribed to %s \n", msg.subscription_entry);
            break;
        case MESSAGE_S2C_KICK:
            printf("[SERVER]");
            switch (msg.kick) {
                case KICK_REASON_CLOSING :
                    printf("Kicked : the server closed the connection.\n");
                    break;
                case KICK_REASON_PROTOCOL_ERROR :
                    printf("Kicked : protocol error.\n");
                    break;

            }
            break;
    }
}

void publish(client_state client){
    message_c2s message = {
        .tag = MESSAGE_C2S_PUBLISH,
    };
    memset(&message.publish, 0, MESSAGE_MAX_LENGTH);
    memcpy(&message.publish, client.cmd+2, strnlen(client.cmd+2, MESSAGE_MAX_LENGTH));

    memset(client.send_buffer, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, client.send_buffer);

    while(client.send_buffer_len != IO_BUFFER_SIZE){
        client.send_buffer_len += send(client.client_socket, client.send_buffer , IO_BUFFER_SIZE, 0);
    }
    client.send_buffer_len = 0;
}

void subscribe(client_state client, bool unsub){
    message_c2s message;
    if (unsub){
        message.tag = MESSAGE_C2S_UNSUBSCRIBE_TO;
    } else {
        message.tag = MESSAGE_C2S_SUBSCRIBE_TO;
    }

    memset(&message.publish, 0, MESSAGE_MAX_LENGTH);
    memcpy(&message.publish, client.cmd+2, strnlen(client.cmd+2, MESSAGE_MAX_LENGTH));

    memset(client.cmd, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, client.send_buffer);

    while(client.send_buffer_len != IO_BUFFER_SIZE){
        client.send_buffer_len += send(client.client_socket, client.send_buffer+client.send_buffer_len , IO_BUFFER_SIZE, 0);
    }
    client.send_buffer_len = 0;
}
