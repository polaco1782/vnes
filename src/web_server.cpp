#include "web_server.h"
#include "bus.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "debugger.h"
#include <crow.h>
#include <sstream>
#include <iomanip>
#include <fstream>

// Helper to convert u8 to hex string
static std::string hexByte(u8 val)
{
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)val;
    return ss.str();
}

// Helper to convert u16 to hex string
static std::string hexWord(u16 val)
{
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << val;
    return ss.str();
}

// Get memory region name
static std::string getMemoryRegion(u16 addr)
{
    if (addr < 0x2000) return "RAM";
    if (addr < 0x4000) return "PPU";
    if (addr < 0x4018) return "APU/IO";
    if (addr < 0x6000) return "EXP";
    if (addr < 0x8000) return "SRAM";
    return "ROM";
}

// GameGenie code decoding for NES
// NES GameGenie formats: 
// - 6 characters: XXXXXX (address: first 3 chars, value: last 3 chars)
// - 8 characters: XXXXXXXX (address: first 4 chars, value: last 4 chars)
// Returns true if decoded successfully, fills addr and value
static bool decodeGameGenie(const std::string& code, u16& addr, u8& value)
{
    // Convert to uppercase and remove any hyphens or spaces
    std::string clean;
    for (char c : code) {
        if (c != '-' && c != ' ') {
            if (c >= 'a' && c <= 'f') clean += c - 32; // to uppercase
            else if (c >= 'A' && c <= 'F' || c >= '0' && c <= '9') clean += c;
            else return false; // invalid character
        }
    }
    
    // NES GameGenie codes are either 6 or 8 characters
    if (clean.length() != 6 && clean.length() != 8) {
        return false;
    }
    
    // Parse based on length
    try {
        if (clean.length() == 6) {
            // 6-char format: XXXXXX (address: first 3, value: last 3)
            addr = (u16)std::stoul(clean.substr(0, 3), nullptr, 16);
            value = (u8)std::stoul(clean.substr(3, 3), nullptr, 16);
        } else {
            // 8-char format: XXXXXXXX (address: first 4, value: last 4)
            addr = (u16)std::stoul(clean.substr(0, 4), nullptr, 16);
            value = (u8)std::stoul(clean.substr(4, 4), nullptr, 16);
        }
    } catch (...) {
        return false;
    }
    
    return true;
}

WebServer::WebServer(Bus* b)
    : bus(b), running(false), debugger_(nullptr)
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
    cmd_cv_.notify_all();
    if (server_thread.joinable()) server_thread.join();
}

void WebServer::pushCommand(Command cmd)
{
    std::lock_guard<std::mutex> lk(cmd_mutex_);
    cmd_queue_.push(std::move(cmd));
    cmd_cv_.notify_one();
}

bool WebServer::processOneCommand(Bus* bus)
{
    Command cmd;
    {
        std::lock_guard<std::mutex> lk(cmd_mutex_);
        if (cmd_queue_.empty()) return false;
        cmd = std::move(cmd_queue_.front());
        cmd_queue_.pop();
    }

    // Process command and set promise if exists
    crow::json::wvalue out;
    switch (cmd.type) {
        case CommandType::GET_ALL:
        {
            out["type"] = "all";

            // cpu
            crow::json::wvalue cpu;
            CPU& cpu_ref = bus->cpu;
            std::ostringstream ss;
            ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << cpu_ref.getPC();
            cpu["pc"] = ss.str(); ss.str(""); ss.clear();
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu_ref.getA();
            cpu["a"] = ss.str(); ss.str(""); ss.clear();
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu_ref.getX();
            cpu["x"] = ss.str(); ss.str(""); ss.clear();
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu_ref.getY();
            cpu["y"] = ss.str(); ss.str(""); ss.clear();
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu_ref.getSP();
            cpu["sp"] = ss.str(); ss.str(""); ss.clear();
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu_ref.getStatus();
            cpu["status"] = ss.str();
            cpu["cycles"] = (unsigned long long)cpu_ref.getCycles();
            out["cpu"] = std::move(cpu);

            // Add disassembly around current PC
            Debugger dbg;
            dbg.connect(bus);
            auto lines = dbg.disassemble(cpu_ref.getPC(), 15);
            crow::json::wvalue dis;
            ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << cpu_ref.getPC();
            dis["pc"] = ss.str();
            for (u32 i = 0; i < lines.size(); ++i) dis["lines"][i] = lines[i];
            out["disasm"] = std::move(dis);
            break;
        }

        case CommandType::STEP:
        {
            int c = cmd.count <= 0 ? 1 : cmd.count;
            for (int i = 0; i < c; i++) bus->cpu.step();

            crow::json::wvalue cpu;
            std::ostringstream ss;
            CPU& cpu_ref = bus->cpu;
            ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << cpu_ref.getPC();
            cpu["pc"] = ss.str(); ss.str(""); ss.clear();
            cpu["cycles"] = (unsigned long long)cpu_ref.getCycles();
            out["type"] = "cpu";
            out["cpu"] = std::move(cpu);

            // disasm
            Debugger dbg;
            dbg.connect(bus);
            auto lines = dbg.disassemble(cpu_ref.getPC(), 15);
            crow::json::wvalue dis;
            ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << cpu_ref.getPC();
            dis["pc"] = ss.str();
            for (u32 i = 0; i < lines.size(); ++i) dis["lines"][i] = lines[i];
            out["disasm"] = std::move(dis);
            break;
        }

        case CommandType::RESET:
            bus->reset(); out["type"] = "cpu"; out["cpu"] = crow::json::wvalue(); break;

        case CommandType::DISASSEMBLE:
        {
            Debugger dbg;
            dbg.connect(bus);
            auto lines = dbg.disassemble(cmd.addr, cmd.count <= 0 ? 10 : cmd.count);
            crow::json::wvalue dis;
            dis["pc"] = ("0x" + hexWord(cmd.addr));
            for (u32 i = 0; i < lines.size(); ++i) dis["lines"][i] = lines[i];
            out["disasm"] = std::move(dis);
            break;
        }

        case CommandType::MEMORY:
        {
            std::ostringstream ss;
            ss << "Memory [" << getMemoryRegion(cmd.addr) << "] $" << std::hex << std::setw(4) << std::setfill('0') << cmd.addr << ":\n\n";
            int count = cmd.count <= 0 ? 256 : cmd.count;
            for (int i = 0; i < count; i += 16) {
                ss << std::hex << std::setw(4) << std::setfill('0') << (cmd.addr + i) << "  ";
                for (int j = 0; j < 16 && (i + j) < count; j++) {
                    u8 val = bus->read(cmd.addr + i + j);
                    ss << std::setw(2) << std::setfill('0') << (int)val << " ";
                }
                ss << " |";
                for (int j = 0; j < 16 && (i + j) < count; j++) {
                    u8 c = bus->read(cmd.addr + i + j);
                    ss << (char)((c >= 32 && c < 127) ? c : '.');
                }
                ss << "|\n";
            }
            out["content"] = ss.str();
            break;
        }

        case CommandType::ADD_BREAKPOINT:
            breakpoints_.insert(cmd.addr); out["type"] = "breakpoints"; break;

        case CommandType::REMOVE_BREAKPOINT:
            breakpoints_.erase(cmd.addr); out["type"] = "breakpoints"; break;

        case CommandType::GAMEGENIE_WRITE:
            bus->write(cmd.addr, cmd.value);
            out["type"] = "gamegenie";
            out["message"] = ("Wrote $" + hexByte(cmd.value) + " to $" + hexWord(cmd.addr) + " [" + getMemoryRegion(cmd.addr) + "]");
            break;

        default:
            out["error"] = "unknown command";
    }

    if (cmd.resp) {
        try {
            auto s = out.dump();
            cmd.resp->set_value(s);
        } catch (...) {
            // If promise already satisfied or broken
        }
    }

    return true;
}

void WebServer::runLoop(int port)
{
    crow::SimpleApp app;

    // Serve static HTML only
    app.route_dynamic("/")([this](const crow::request& /*req*/) {
        std::ifstream ifs("web_debugger.html");
        if (!ifs) {
            crow::response r("<html><body><h1>web_debugger.html not found</h1></body></html>");
            r.set_header("Content-Type", "text/html; charset=utf-8");
            return r;
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        crow::response r(content);
        r.set_header("Content-Type", "text/html; charset=utf-8");
        return r;
    });

    // websocket
    app.route_dynamic("/ws").websocket(&app)
        .onopen([this](crow::websocket::connection& conn) {
            CROW_LOG_DEBUG << "WebSocket connection opened";
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            auto msg = crow::json::load(data);
            if (!msg) return;
            std::string cmd = msg.has("cmd") ? msg["cmd"].s() : std::string();

            // Build a command and a promise to get the result back from main thread
            Command c;
            c.resp = std::make_shared<std::promise<std::string>>();
            auto fut = c.resp->get_future();

            if (cmd == "getAll") c.type = CommandType::GET_ALL;
            else if (cmd == "step") { c.type = CommandType::STEP; c.count = msg.has("count") ? msg["count"].i() : 1; }
            else if (cmd == "reset") c.type = CommandType::RESET;
            else if (cmd == "disassemble") { c.type = CommandType::DISASSEMBLE; c.addr = (u16)(msg.has("addr") ? msg["addr"].i() : 0); c.count = msg.has("count") ? msg["count"].i() : 10; }
            else if (cmd == "memory") { c.type = CommandType::MEMORY; c.addr = (u16)(msg.has("addr") ? msg["addr"].i() : 0); c.count = msg.has("count") ? msg["count"].i() : 256; }
            else if (cmd == "addBreakpoint") { c.type = CommandType::ADD_BREAKPOINT; c.addr = (u16)(msg.has("addr") ? msg["addr"].i() : 0); }
            else if (cmd == "removeBreakpoint") { c.type = CommandType::REMOVE_BREAKPOINT; c.addr = (u16)(msg.has("addr") ? msg["addr"].i() : 0); }
            else if (cmd == "gamegenie") { 
            std::string ggcode = msg.has("code") ? msg["code"].s() : std::string();
            if (!ggcode.empty() && decodeGameGenie(ggcode, c.addr, c.value)) {
                c.type = CommandType::GAMEGENIE_WRITE;
            } else {
                // Invalid code, send error response
                if (c.resp) {
                    crow::json::wvalue error;
                    error["error"] = "Invalid GameGenie code format. Use 6 characters (XXXXXX) or 8 characters (XXXXXXXX)";
                    c.resp->set_value(error.dump());
                }
                return;
            }
        }
            else return;

            // Push command and wait for response (with timeout)
            pushCommand(c);
            if (fut.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready) {
                try {
                    auto resp = fut.get();
                    conn.send_text(resp);
                } catch (...) {}
            } else {
                // not ready yet - send ack to client
                conn.send_text(std::string("{\"status\":\"queued\"}"));
            }
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, u16 status) {
            CROW_LOG_DEBUG << "WebSocket connection closed";
        });

    app.port(port).multithreaded().run();
}
