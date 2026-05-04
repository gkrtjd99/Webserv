#include "Config.hpp"
#include "EventLoop.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <string>

namespace
{
	volatile sig_atomic_t g_shutdownRequested = 0;

	void handleShutdownSignal(int)
	{
		g_shutdownRequested = 1;
	}
}

int main(int argc, char** argv)
{
	Config config;

	if (argc > 2)
	{
		std::cerr << "usage: ./webserv [configuration file]" << std::endl;
		return EXIT_FAILURE;
	}
	try
	{
		const std::string configPath = (argc == 2)
			? std::string(argv[1]) : std::string("config/default.conf");

		if (configPath.empty())
			throw std::runtime_error("empty configuration file path");
		config = Config::parse(configPath);
		if (config.servers.empty())
			throw std::runtime_error("no server configuration");
		if (signal(SIGINT, handleShutdownSignal) == SIG_ERR
				|| signal(SIGTERM, handleShutdownSignal) == SIG_ERR)
			throw std::runtime_error("signal setup failed");
		EventLoop eventLoop(config);

		eventLoop.run(&g_shutdownRequested);
	}
	catch (const std::exception& error)
	{
		std::cerr << "webserv: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
