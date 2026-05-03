#include "Config.hpp"

#include "ConfigParser.hpp"
#include "ConfigValidator.hpp"

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

Config Config::parse(const std::string& path)
{
	ConfigParser parser(path);
	Config cfg = parser.parse();

	ConfigValidator validator(cfg);
	validator.run();

	return cfg;
}
