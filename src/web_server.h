#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "types.h"
#include "bus.h"
#include <thread>
#include <atomic>

class WebServer {
public:
    explicit WebServer(Bus* bus);
    ~WebServer();

    // Start server on given port (default 18080)
    void start(int port = 18080);
    void stop();

private:
    Bus* bus;
    std::thread server_thread;
    std::atomic<bool> running;

    void runLoop(int port);
};

#endif // WEB_SERVER_H
