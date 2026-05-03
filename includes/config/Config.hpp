#ifndef WEBSERV_CONFIG_CONFIG_HPP
#define WEBSERV_CONFIG_CONFIG_HPP

#include <string>
#include <vector>

#include "ServerConfig.hpp"

struct Config {
	std::vector<ServerConfig> servers;

	static Config parse(const std::string& path);
};

#endif
