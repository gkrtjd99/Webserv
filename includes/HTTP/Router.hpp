#ifndef ROUTER_HPP
# define ROUTER_HPP

#include "Config/Config.hpp"

#include <string>
#include <vector>

const ServerConfig* matchServer(const std::vector<ServerConfig>& servers,
		const std::string& host,
		int port);

const LocationConfig* matchLocation(const ServerConfig& server,
		const std::string& path);

#endif
