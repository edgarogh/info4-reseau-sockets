#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "codec.h"

char buffer[IO_BUFFER_SIZE];

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
        .sin_port = port,
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
    memcpy(&message.join_as, user, strnlen(user, MESSAGE_MAX_LENGTH));
    memset(buffer, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, buffer);
    size_t i = 0;
    while(i != IO_BUFFER_SIZE){
        i += send(client_socket, buffer+i , IO_BUFFER_SIZE, 0);
    }
    printf("[INFO] Logged in as %s \n", user);
}

int publish(int socket, char* txt){
    message_c2s message = {
        .tag = MESSAGE_C2S_PUBLISH,
    };
    memset(&message.publish, 0, MESSAGE_MAX_LENGTH);
    memcpy(&message.publish, txt, strnlen(txt, MESSAGE_MAX_LENGTH));

    memset(buffer, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, buffer);

    size_t i = 0;
    while(i != IO_BUFFER_SIZE){
        i += send(socket, buffer+i , IO_BUFFER_SIZE, 0);
    }
}

int subscribe(int socket, char* usr, bool unsub){
    message_c2s message;
    if (unsub){
        message.tag = MESSAGE_C2S_UNSUBSCRIBE_TO;
    } else {
        message.tag = MESSAGE_C2S_SUBSCRIBE_TO;
    }

    memset(&message.publish, 0, MESSAGE_MAX_LENGTH);
    memcpy(&message.publish, usr, strnlen(usr, MESSAGE_MAX_LENGTH));

    memset(buffer, 0, MESSAGE_MAX_LENGTH);
    encode_c2s(&message, buffer);

    size_t i = 0;
    while(i != IO_BUFFER_SIZE){
        i += send(socket, buffer+i , IO_BUFFER_SIZE, 0);
    }
}
