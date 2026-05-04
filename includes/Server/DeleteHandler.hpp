#ifndef DELETEHANDLER_HPP
# define DELETEHANDLER_HPP

#include "Config.hpp"
#include "FileResource.hpp"
#include "HttpRequest.hpp"
#include "PathResolver.hpp"
#include "ResponseBuilder.hpp"

#include <string>

class DeleteHandler
{
public:
	DeleteHandler(const PathResolver& paths,
			const FileResource& files,
			const ResponseBuilder& responses);

	std::string handle(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive) const;

private:
	const PathResolver& _paths;
	const FileResource& _files;
	const ResponseBuilder& _responses;
};

#endif
