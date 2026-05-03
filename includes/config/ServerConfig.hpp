#ifndef WEBSERV_CONFIG_SERVER_CONFIG_HPP
#define WEBSERV_CONFIG_SERVER_CONFIG_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LocationConfig.hpp"

struct ServerConfig {
	std::string                 host;
	int                         port;
	std::vector<std::string>    serverNames;
	std::map<int, std::string>  errorPages;
	std::size_t                 clientMaxBodySize;
	std::vector<LocationConfig> locations;

	ServerConfig();
};

#endif
