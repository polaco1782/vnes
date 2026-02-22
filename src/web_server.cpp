#include "web_server.h"
#include "bus.h"
#include "cpu.h"
#include "debugger.h"
#include <crow.h>
#include <sstream>
#include <iomanip>

WebServer::WebServer(Bus* b)
    : bus(b), running(false)
{
}

WebServer::~WebServer()
{
    stop();
}

void WebServer::start(int port)
{
    if (running) return;
    running = true;
    server_thread = std::thread(&WebServer::runLoop, this, port);
}

void WebServer::stop()
{
    if (!running) return;
    running = false;
    if (server_thread.joinable()) server_thread.join();
}

void WebServer::runLoop(int port)
{
    crow::SimpleApp app;

    // Register CPU endpoint
    app.route_dynamic("/cpu")([this](const crow::request& /*req*/) {
        crow::json::wvalue res;
        CPU& cpu = bus->cpu;
        std::ostringstream ss;
        ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.getPC();
        res["pc"] = ss.str();
        ss.str(""); ss.clear();
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.getA();
        res["a"] = ss.str();
        ss.str(""); ss.clear();
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.getX();
        res["x"] = ss.str();
        ss.str(""); ss.clear();
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.getY();
        res["y"] = ss.str();
        ss.str(""); ss.clear();
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.getSP();
        res["sp"] = ss.str();
        ss.str(""); ss.clear();
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.getStatus();
        res["status"] = ss.str();
        res["cycles"] = (unsigned long long)cpu.getCycles();
        return res;
    });

    // Serve simple debugger HTML
    app.route_dynamic("/")([this](const crow::request& /*req*/) {
        const char* html =
            "<!doctype html>"
            "<html><head><meta charset=\"utf-8\"><title>vNES Debugger</title>"
            "<style>body{font-family:monospace;padding:20px;}button{padding:8px 12px;}pre{background:#222;color:#bada55;padding:10px;}</style>"
            "</head><body>"
            "<h1>vNES Debugger</h1>"
            "<button id=dis>Disassemble PC</button>"
            "<pre id=out></pre>"
            "<script>document.getElementById('dis').addEventListener('click',function(){fetch('/disassemble').then(r=>r.json()).then(j=>{document.getElementById('out').textContent = j.pc + '\n' + j.lines.join('\n');});});</script>"
            "</body></html>";
        crow::response r(html);
        r.set_header("Content-Type", "text/html; charset=utf-8");
        return r;
    });

    // Disassemble endpoint: returns JSON with PC and lines
    app.route_dynamic("/disassemble")([this](const crow::request& /*req*/) {
        crow::json::wvalue res;
        // Use a temporary Debugger to produce disassembly using existing logic
        Debugger dbg;
        dbg.connect(bus);
        u16 pc = bus->cpu.getPC();
        int count = 10;
        auto lines = dbg.disassemble(pc, count);
        std::ostringstream ss;
        ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << pc;
        res["pc"] = ss.str();
        for (size_t i = 0; i < lines.size(); ++i) {
            res["lines"][i] = lines[i];
        }
        return res;
    });

    app.port(port).multithreaded().run();
}
