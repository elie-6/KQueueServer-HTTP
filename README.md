# KQueue HTTP Server

A **minimal, event-driven HTTP server** in modern C++, built directly on top of OS primitives.

Implements the core mechanics of high-performance servers — non-blocking sockets, kernel event queues, and controlled concurrency — without frameworks or abstractions.

This project exists to expose how real servers actually work under the hood.

---

## Architecture
```
            +-------------------+
            |   Main Thread     |
            |  (Event Loop)     |
            +---------+---------+
                      |
                      v
            +-------------------+
            |      kqueue       |
            |  (read readiness) |
            +---------+---------+
                      |
           client fd ready for read
                      |
                      v
            +-------------------+
            |   Thread Pool     |
            | (fixed workers)   |
            +---------+---------+
                      |
                      v
            +-------------------+
            |  handleClient()   |
            |  read / respond   |
            +-------------------+
```

---

## Overview

The server accepts incoming TCP connections and processes HTTP requests using a **reactor + thread pool architecture**:

- **Non-blocking listening socket**
- **kqueue-based event loop** for readiness notifications
- **Accept-drain loop** to handle bursts efficiently
- **Thread pool** for bounded parallel request handling
- **Graceful shutdown** via signal handling
- **HTTP/1.1 keep-alive** support

The focus is correctness, clarity, and OS-level behavior — not full HTTP compliance.

---

## Tech Stack

**Language**: C++17  
**Platform**: macOS / BSD  
**Event API**: kqueue  
**Concurrency**: POSIX threads (via `std::thread`)

**Core Components**:
- `Server` — socket setup, event loop, lifecycle management
- `ThreadPool` — fixed worker pool with task queue
- `handleClient()` — per-connection read/write logic
- Signal handling — graceful shutdown coordination

---

## Project Structure
```
src/
  ├─ main.cpp            # entry point + signal wiring
  ├─ Server.hpp
  ├─ Server.cpp          # socket + kqueue event loop
  ├─ ThreadPool.hpp
  └─ ThreadPool.cpp      # worker pool implementation

build/
  └─ server              # compiled binary

.vscode/
  ├─ tasks.json          # build task
  └─ launch.json         # debugger config
```

---

## Quick Start

### Build
```bash
clang++ -std=c++17 src/*.cpp -pthread -o build/server
```

### Run
```bash
./build/server
```

### Test
```bash
curl http://localhost:8080
```

---

## Server Lifecycle

### Startup

1. Create listening socket
2. Set `SO_REUSEADDR`
3. Enable non-blocking mode
4. Bind + listen on port
5. Initialize `kqueue`
6. Register listening fd for `EVFILT_READ`

### Runtime

`kevent()` blocks until:
- A new client connects
- A client socket becomes readable

**Listening fd**:
- Accept all pending connections until `EAGAIN`
- Set each client socket non-blocking
- Register client fd with `kqueue`

**Client fd**:
- Removed from `kqueue`
- Handed to the thread pool for processing

### Shutdown

1. `SIGINT` (Ctrl+C) triggers `Server::stop()`
2. Event loop exits cleanly
3. Thread pool drains and joins workers
4. All sockets are closed deterministically

---

## Threading Model

**One main thread owns**:
- `kqueue`
- Accept loop
- fd registration

**Fixed worker pool processes client I/O**:
- No fd is ever read by two threads concurrently

This avoids race conditions while keeping the event loop fast and deterministic.

---

## I/O Semantics

### Non-Blocking Reads

`read()` is called in a loop until:
- Data is exhausted
- Or `EAGAIN` / `EWOULDBLOCK` occurs

When `EAGAIN` is hit:
- The fd is re-registered with `kqueue`
- The worker returns control to the event loop

### Connection Handling

- `read() == 0` → client closed connection
- `Connection: close` header → close after response
- Otherwise, HTTP/1.1 keep-alive is honored

### Error Handling

- **EINTR**: safely ignored, event loop retries without spinning
- **EAGAIN / EWOULDBLOCK**: treated as normal non-blocking behavior
- **All other syscall errors**: logged, connection closed deterministically

---

## Design Highlights

**Event-driven core**: `kqueue` is used directly to avoid polling and busy loops. The kernel tells the server exactly when work is available.

**Accept-drain strategy**: A single readiness event may represent many pending connections. The accept loop drains until `EAGAIN` to avoid starvation.

**Delete-before-dispatch**: Client fds are removed from `kqueue` before being processed by a worker, guaranteeing exclusive ownership.

**Bounded concurrency**: A fixed thread pool prevents unbounded thread creation under load.

**Graceful shutdown**: Signal handling sets a stop flag instead of interrupting execution paths. All threads exit cleanly.

---

## Known Limitations

- **No full HTTP parser** (headers parsed minimally)
- **No partial-write retry loop**
- **No request pipelining**
- **No TLS**
- **kqueue-specific** (macOS / BSD only)

These are intentional omissions to keep the focus on core server mechanics.

---

## Why This Matters

This project demonstrates:

- **How production servers are structured internally**
- **Why non-blocking I/O is mandatory for scalability**
- **How kernel event APIs interact with user-space threading**
- **How to safely coordinate signals, syscalls, and concurrency**

The same architectural principles apply to Nginx, Envoy, and high-performance backend systems — stripped down here to their essentials.

---

