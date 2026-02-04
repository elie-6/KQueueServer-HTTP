#pragma once
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <mutex>
#include "ThreadPool.hpp" 


class Server {
private:
    int listen_fd;
    int kq;
    sockaddr_in server_addr;
    ThreadPool pool; 
    bool running = true; 

public:
    Server(int port, size_t thread_count);
    ~Server();
    void run();
    void handleClient(int client_fd);
     void stop() { running = false; } 
    
};
