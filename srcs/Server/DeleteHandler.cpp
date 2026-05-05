#include "DeleteHandler.hpp"

#include <sys/stat.h>

DeleteHandler::DeleteHandler(const PathResolver& paths,
		const FileResource& files,
		const ResponseBuilder& responses)
	: _paths(paths),
	  _files(files),
	  _responses(responses)
{
}

std::string DeleteHandler::handle(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string path = _paths.buildFilePath(location, request.path());
	struct stat info;

	if(path.empty())
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(stat(path.c_str(), &info) < 0)
	{
		return _responses.buildError(404, &server, keepAlive);
	}
	if(!S_ISREG(info.st_mode))
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(!_paths.resolveInsideRoot(location, path, path))
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	if(!_files.removeRegularFile(path))
	{
		return _responses.buildError(403, &server, keepAlive);
	}
	return _responses.build(204, "", "text/plain", keepAlive);
}
