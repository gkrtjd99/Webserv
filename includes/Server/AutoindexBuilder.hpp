#ifndef AUTOINDEXBUILDER_HPP
# define AUTOINDEXBUILDER_HPP

#include "HttpRequest.hpp"

#include <string>

class AutoindexBuilder
{
public:
	bool buildBody(const HttpRequest& request,
			const std::string& directoryPath,
			std::string& body) const;

private:
	std::string htmlEscape(const std::string& value) const;
};

#endif
