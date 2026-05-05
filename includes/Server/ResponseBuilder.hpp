#ifndef RESPONSEBUILDER_HPP
# define RESPONSEBUILDER_HPP

#include "Config.hpp"

#include <set>
#include <string>

class ResponseBuilder
{
public:
	std::string build(int status,
			const std::string& body,
			const std::string& contentType,
			bool keepAlive) const;
	std::string buildWithHeaders(int status,
			const std::string& body,
			const std::string& contentType,
			const std::string& extraHeaders,
			bool keepAlive) const;
	std::string buildError(int status,
			const ServerConfig* server,
			bool keepAlive) const;
	std::string buildMethodNotAllowed(const LocationConfig& location,
			bool keepAlive) const;
	std::string buildRedirect(const LocationConfig& location,
			bool keepAlive) const;
	const char* statusReason(int status) const;
	const char* contentTypeForPath(const std::string& path) const;

private:
	std::string methodSetToAllowHeader(
			const std::set<HttpMethod>& methods) const;
};

#endif
