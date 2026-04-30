#include "Router.hpp"
#include "Config/Config.hpp"

namespace
{
	bool hasServerName(const ServerConfig& server, const std::string& host)
	{
		for(std::size_t i = 0; i < server.serverNames.size(); i++)
		{
			if(server.serverNames[i] == host)
			{
				return true;
			}
		}
		return false;
	}

	bool locationMatches(const std::string& requestPath,
			const std::string& locationPath)
	{
		if(locationPath.empty())
		{
			return false;
		}

		if(locationPath == "/")
		{
			return (!requestPath.empty()) && (requestPath[0] == '/');
		}
		
		if(requestPath.compare(0, locationPath.size(), locationPath) != 0)
		{
			return false;
		}

		if(requestPath.size() == locationPath.size())
		{
			return true;
		}

		return requestPath[locationPath.size()] == '/';
	}
}

const ServerConfig* matchServer(const std::vector<ServerConfig>& servers,
		const std::string& host,
		int port)
{
	const ServerConfig* defaultServer = NULL;

	for(std::size_t i = 0; i < servers.size(); i++)
	{
		if(servers[i].port != port)
		{
			continue;
		}

		if(defaultServer == NULL)
		{
			defaultServer = &servers[i];
		}

		if(hasServerName(servers[i], host))
		{
			return &servers[i];
		}
	}

	return defaultServer;
}

const LocationConfig* matchLocation(const ServerConfig& server,
		const std::string& path)
{
	const LocationConfig* best = NULL;
	std::size_t bestLen = 0;

	for(std::size_t i = 0; i < server.locations.size(); i++)
	{
		const std::string& locationPath = server.locations[i].path;

		if(locationMatches(path, locationPath) && locationPath.size() > bestLen)
		{
			best = &server.locations[i];
			bestLen = locationPath.size();
		}
	}

	return best;
}

