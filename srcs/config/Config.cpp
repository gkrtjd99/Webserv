#include "Config.hpp"

#include "HttpMethod.hpp"

LocationConfig::LocationConfig()
	: path()
	, root()
	, index()
	, autoindex(false)
	, methods()
	, redirect(0, std::string())
	, uploadStore()
	, cgi()
	, clientMaxBodySize(0)
{
}

ServerConfig::ServerConfig()
	: host()
	, port(0)
	, serverNames()
	, errorPages()
	, clientMaxBodySize(0)
	, locations()
{
}

Config Config::parse(const std::string& /*path*/)
{
	// M0 stub: 하드코딩된 1-server / 1-location 설정.
	// 실제 파일 파싱은 PR-3 이후 ConfigLexer/Parser/Validator 가 도입되며 교체된다.
	Config cfg;

	ServerConfig server;
	server.host = "0.0.0.0";
	server.port = 8080;
	server.clientMaxBodySize = 0;

	LocationConfig loc;
	loc.path = "/";
	loc.root = "./www";
	loc.autoindex = false;
	loc.methods.insert(HTTP_GET);

	server.locations.push_back(loc);
	cfg.servers.push_back(server);

	return cfg;
}
