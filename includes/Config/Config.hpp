#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <string>
#include <vector>

struct LocationConfig
{
	std::string path;
};

struct ServerConfig
{
	int port;
	std::vector<std::string> serverNames;
	std::vector<LocationConfig> locations;
};

#endif
