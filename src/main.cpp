#include "Server.hpp"
#include <iostream>
#include <csignal>

Server* server_ptr = nullptr; 

void handle_sigint(int) {
    if (server_ptr)
        server_ptr->stop();  // tell server to stop
}

int main() {
    size_t thread_count = 4; // number of worker threads in ThreadPool
    Server server(8080, thread_count);  // listen on port 8080 with 4 threads
    server_ptr = &server;
    signal(SIGINT, handle_sigint);   // handle Ctrl+C for graceful shutdown
    server.run();
    return 0;
}