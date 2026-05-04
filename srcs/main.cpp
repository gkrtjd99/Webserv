#include "Config.hpp"
#include "EventLoop.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

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
		EventLoop eventLoop(config);

		eventLoop.run();
	}
	catch (const std::exception& error)
	{
		std::cerr << "webserv: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
