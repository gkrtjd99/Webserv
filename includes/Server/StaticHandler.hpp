#ifndef STATICHANDLER_HPP
# define STATICHANDLER_HPP

#include "AutoindexBuilder.hpp"
#include "Config.hpp"
#include "FileResource.hpp"
#include "HttpRequest.hpp"
#include "PathResolver.hpp"
#include "ResponseBuilder.hpp"

#include <string>

class StaticHandler
{
public:
	StaticHandler(const PathResolver& paths,
			const FileResource& files,
			const AutoindexBuilder& autoindex,
			const ResponseBuilder& responses);

	std::string handle(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive) const;

private:
	const PathResolver& _paths;
	const FileResource& _files;
	const AutoindexBuilder& _autoindex;
	const ResponseBuilder& _responses;
};

#endif
