#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "types.h"
#include <thread>
#include <atomic>

// Simplified web server - serves static HTML only
class WebServer {
public:
    WebServer();
    ~WebServer();

    // Start server on given port (default 18080)
    void start(int port = 18080);
    void stop();

private:
    std::thread server_thread;
    std::atomic<bool> running;

    void runLoop(int port);
};

#endif // WEB_SERVER_H
