#ifndef PATHRESOLVER_HPP
# define PATHRESOLVER_HPP

#include "CgiTypes.hpp"
#include "Config.hpp"
#include "HttpRequest.hpp"

#include <string>

class PathResolver
{
public:
	std::string joinPath(const std::string& left,
			const std::string& right) const;
	std::string stripLocationPrefix(const LocationConfig& location,
			const std::string& requestPath) const;
	bool normalizeRelativePath(const std::string& input,
			std::string& output) const;
	bool resolveInsideRoot(const LocationConfig& location,
			const std::string& rawPath,
			std::string& resolvedPath) const;
	std::string buildFilePath(const LocationConfig& location,
			const std::string& requestPath) const;
	std::string buildFilePathFromUri(const ServerConfig& server,
			const std::string& uriPath) const;
	bool findCgiScript(const HttpRequest& request,
			const LocationConfig& location,
			CgiMatch& match) const;

private:
	bool endsWith(const std::string& value,
			const std::string& suffix) const;
};

#endif
