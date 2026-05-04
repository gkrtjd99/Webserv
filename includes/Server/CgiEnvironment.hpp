#ifndef CGIENVIRONMENT_HPP
# define CGIENVIRONMENT_HPP

#include "CgiTypes.hpp"
#include "Config.hpp"
#include "HttpRequest.hpp"

#include <string>
#include <vector>

class CgiEnvironment
{
public:
	std::vector<std::string> build(const HttpRequest& request,
			const ServerConfig& server,
			const CgiMatch& match,
			const std::string& remoteAddress) const;
	void buildCharArray(const std::vector<std::string>& strings,
			std::vector<char*>& result) const;

private:
	void add(std::vector<std::string>& env,
			const std::string& name,
			const std::string& value) const;
	std::string toString(std::size_t value) const;
	std::string toString(int value) const;
	std::string toUpperHeaderName(const std::string& value) const;
};

#endif
