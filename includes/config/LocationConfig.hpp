#ifndef WEBSERV_CONFIG_LOCATION_CONFIG_HPP
#define WEBSERV_CONFIG_LOCATION_CONFIG_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "HttpMethod.hpp"

struct LocationConfig {
	std::string                        path;
	std::string                        root;
	std::string                        index;
	bool                               autoindex;
	std::set<HttpMethod>               methods;
	std::pair<int, std::string>        redirect;
	std::string                        uploadStore;
	std::map<std::string, std::string> cgi;
	std::size_t                        clientMaxBodySize;

	LocationConfig();
};

#endif
