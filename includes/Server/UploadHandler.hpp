#ifndef UPLOADHANDLER_HPP
# define UPLOADHANDLER_HPP

#include "Config.hpp"
#include "HttpRequest.hpp"
#include "PathResolver.hpp"
#include "ResponseBuilder.hpp"

#include <cstddef>
#include <string>

class UploadHandler
{
public:
	UploadHandler(const PathResolver& paths,
			const ResponseBuilder& responses);

	std::string handle(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive);

private:
	const PathResolver& _paths;
	const ResponseBuilder& _responses;
	std::size_t _sequence;

	std::string toString(std::size_t value) const;
};

#endif
