#include "web_server.h"
#include <crow.h>
#include <fstream>

WebServer::WebServer()
	: running(false)
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

	// Serve static HTML only
	app.route_dynamic("/")([](const crow::request& /*req*/) {
		std::ifstream ifs("web_debugger.html");
		if (!ifs) {
			crow::response r("<html><body><h1>web_debugger.html not found</h1><p>Debugging is now done through the in-app GUI console (press ESC then Debug > Console)</p></body></html>");
			r.set_header("Content-Type", "text/html; charset=utf-8");
			return r;
		}

		std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		crow::response r(content);
		r.set_header("Content-Type", "text/html; charset=utf-8");
		return r;
	});

	app.port(port).multithreaded().run();
}
