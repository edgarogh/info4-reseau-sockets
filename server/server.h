typedef struct {
    int server_socket;
    int epoll;
} server_state;

void handle_event(server_state* server, struct epoll_event* event);
