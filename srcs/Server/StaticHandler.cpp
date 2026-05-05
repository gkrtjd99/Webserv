#include "StaticHandler.hpp"

#include <sys/stat.h>
#include <unistd.h>

StaticHandler::StaticHandler(const PathResolver& paths,
		const FileResource& files,
		const AutoindexBuilder& autoindex,
		const ResponseBuilder& responses)
	: _paths(paths),
	  _files(files),
	  _autoindex(autoindex),
	  _responses(responses)
{
}

std::string StaticHandler::handle(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string path;
	std::string body;
	struct stat info;
	std::string tail = _paths.stripLocationPrefix(location, request.path());
	std::string normalizedTail;

	if((tail.empty()
				|| (!request.path().empty()
					&& request.path()[request.path().size() - 1] == '/'))
			&& _paths.normalizeRelativePath(tail, normalizedTail))
	{
		std::string directoryPath = normalizedTail.empty()
			? location.root : _paths.joinPath(location.root, normalizedTail);

		if(stat(directoryPath.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
		{
			path = _paths.joinPath(directoryPath, location.index);
			if(stat(path.c_str(), &info) < 0)
			{
				if(location.autoindex)
				{
					if(_autoindex.buildBody(request, directoryPath, body))
					{
						return _responses.build(200, body, "text/html",
								keepAlive);
					}
				}
				return _responses.buildError(403, &server, keepAlive);
			}
		}
	}

	if(path.empty())
	{
		path = _paths.buildFilePath(location, request.path());
	}
	if(path.empty())
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(stat(path.c_str(), &info) < 0)
	{
		return _responses.buildError(404, &server, keepAlive);
	}
	if(S_ISDIR(info.st_mode))
	{
		std::string directoryPath = path;

		path = _paths.buildFilePath(location, request.path() + "/");
		if(path.empty() || stat(path.c_str(), &info) < 0)
		{
			if(location.autoindex)
			{
				if(_autoindex.buildBody(request, directoryPath, body))
				{
					return _responses.build(200, body, "text/html",
							keepAlive);
				}
			}
			return _responses.buildError(403, &server, keepAlive);
		}
	}
	if(!S_ISREG(info.st_mode))
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(!_paths.resolveInsideRoot(location, path, path))
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(access(path.c_str(), R_OK) < 0)
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(!_files.readRegularFile(path, body))
	{
		return _responses.buildError(500, &server, keepAlive);
	}
	return _responses.build(200, body, _responses.contentTypeForPath(path),
			keepAlive);
}
