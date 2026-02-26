#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "types.h"
#include "bus.h"
#include "debugger.h"
#include <thread>
#include <atomic>
#include <set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <string>

class WebServer {
public:
    explicit WebServer();
    ~WebServer();

    // Start server on given port (default 18080)
    void start(int port = 18080);
    void stop();

    // Command queue used to send requests from websocket thread to main emulation thread
    enum class CommandType {
        GET_ALL,
        STEP,
        RESET,
        DISASSEMBLE,
        MEMORY,
        ADD_BREAKPOINT,
        REMOVE_BREAKPOINT,
        GAMEGENIE_WRITE,
        NONE
    };

    struct Command {
        CommandType type = CommandType::NONE;
        u16 addr = 0;
        u8 value = 0;
        int count = 0;
        std::string ggcode;
        // promise to deliver JSON string result back to websocket handler
        std::shared_ptr<std::promise<std::string>> resp;
    };

    // Push a command into the queue (called from websocket thread)
    void pushCommand(Command cmd);

    // Called from main emulation thread to process at least one pending command. Returns true if a command was processed.
    bool processOneCommand(Bus* bus);

private:
    Bus* bus;
    std::thread server_thread;
    std::atomic<bool> running;
    Debugger* debugger_;
    std::set<u16> breakpoints_;

    std::queue<Command> cmd_queue_;
    std::mutex cmd_mutex_;
    std::condition_variable cmd_cv_;

    void runLoop(int port);
};

#endif // WEB_SERVER_H
