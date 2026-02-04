#include "Server.hpp"
#include <iostream>
#include <cstring>
#include <fcntl.h>


void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Server::Server(int port, size_t thread_count) 
    : pool(thread_count), running(true) 
{
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlocking(listen_fd);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }

    kq = kqueue();
    if (kq < 0) {
        perror("kqueue");
        exit(1);
    }

    std::cout << "Server listening on port " << port << std::endl;
}

Server::~Server() {
    close(listen_fd);
}

void Server::run() {
    struct kevent change;
    struct kevent events[10];

    // Register listen_fd
    EV_SET(&change, listen_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &change, 1, NULL, 0, NULL);

    std::cout << "Event loop started" << std::endl;

    while (running) {
        int nev = kevent(kq, NULL, 0, events, 10, NULL);
        if (nev < 0) {
            if (errno == EINTR)  
               continue;  
            perror("kevent");
            break;
        }

        for (int i = 0; i < nev; i++) {

            int fd = events[i].ident;

           
            if (fd == listen_fd) {
                while (true) {
                    int client_fd = accept(listen_fd, NULL, NULL);

                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // no more clients
                        if (errno == EINTR)
                            continue;
                        perror("accept");
                        break;
                    }

                    setNonBlocking(client_fd);
                    std::cout << "Accepted client fd: " << client_fd << std::endl;

                    // Register client fd for READ events
                    EV_SET(&change, client_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                    kevent(kq, &change, 1, NULL, 0, NULL);
                }
            }

            
             else {
                // remove fd from kqueue to prevent double-processing
                EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                kevent(kq, &change, 1, NULL, 0, NULL);

                // send work to thread pool
                pool.enqueue([this, fd]() {
                    handleClient(fd);
                });
            }
        }
    }

    close(listen_fd);
    std::cout << "Server shutting down gracefully." << std::endl;
}


void Server::handleClient(int client_fd) {
    constexpr size_t buffer_size = 4096;
    char buffer[buffer_size];

    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, buffer_size - 1);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct kevent change;
                EV_SET(&change, client_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
                kevent(kq, &change, 1, NULL, 0, NULL);
                break;
            } else {
                perror("read");
                break;
            }
        } else if (bytes_read == 0) {
            // Client closed connection
            break;
        }

        buffer[bytes_read] = '\0';
        std::string request(buffer);

        std::cout << "Received request from fd " << client_fd << ":\n" << request << std::endl;

        const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nHello World\n";
        ssize_t written = write(client_fd, response, strlen(response));
        if (written < 0) {
            perror("write");
            break;
        }

        if (request.find("Connection: close") != std::string::npos) {
            break;
        }

    }

    close(client_fd);
}