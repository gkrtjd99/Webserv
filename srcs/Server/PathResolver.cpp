#include "PathResolver.hpp"
#include "Router.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <vector>

std::string PathResolver::joinPath(const std::string& left,
		const std::string& right) const
{
	if(left.empty())
	{
		return right;
	}
	if(right.empty())
	{
		return left;
	}
	if(left[left.size() - 1] == '/')
	{
		if(right[0] == '/')
		{
			return left + right.substr(1);
		}
		return left + right;
	}
	if(right[0] == '/')
	{
		return left + right;
	}
	return left + "/" + right;
}

std::string PathResolver::stripLocationPrefix(const LocationConfig& location,
		const std::string& requestPath) const
{
	std::string tail = requestPath;

	if(location.path != "/" && requestPath.compare(0,
				location.path.size(), location.path) == 0)
	{
		tail = requestPath.substr(location.path.size());
	}
	if(!tail.empty() && tail[0] == '/')
	{
		tail.erase(0, 1);
	}
	return tail;
}

bool PathResolver::normalizeRelativePath(const std::string& input,
		std::string& output) const
{
	std::vector<std::string> parts;
	std::size_t begin = 0;

	while(begin <= input.size())
	{
		std::size_t slash = input.find('/', begin);
		std::size_t end = slash == std::string::npos ? input.size() : slash;
		std::string part = input.substr(begin, end - begin);

		if(part.empty() || part == ".")
		{
		}
		else if(part == "..")
		{
			if(parts.empty())
			{
				return false;
			}
			parts.pop_back();
		}
		else
		{
			parts.push_back(part);
		}
		if(slash == std::string::npos)
		{
			break;
		}
		begin = slash + 1;
	}
	output.clear();
	for(std::size_t i = 0; i < parts.size(); i++)
	{
		if(i != 0)
		{
			output += "/";
		}
		output += parts[i];
	}
	return true;
}

bool PathResolver::resolveInsideRoot(const LocationConfig& location,
		const std::string& rawPath,
		std::string& resolvedPath) const
{
	std::string root = location.root;
	std::string tail;
	std::string normalized;

	if(root.empty())
	{
		return false;
	}
	if(root != "/" && rawPath == root)
	{
		resolvedPath = root;
		return true;
	}
	if(root == "/")
	{
		if(rawPath.empty() || rawPath[0] != '/')
		{
			return false;
		}
		tail = rawPath.substr(1);
	}
	else
	{
		if(rawPath.size() <= root.size()
				|| rawPath.compare(0, root.size(), root) != 0
				|| rawPath[root.size()] != '/')
		{
			return false;
		}
		tail = rawPath.substr(root.size() + 1);
	}
	if(!normalizeRelativePath(tail, normalized))
	{
		return false;
	}
	if(normalized.empty())
	{
		resolvedPath = root;
	}
	else
	{
		resolvedPath = joinPath(root, normalized);
	}
	return true;
}

std::string PathResolver::buildFilePath(const LocationConfig& location,
		const std::string& requestPath) const
{
	std::string tail;
	std::string normalized;

	if(requestPath.empty() || requestPath[0] != '/')
	{
		return "";
	}
	tail = stripLocationPrefix(location, requestPath);
	if(tail.empty() || tail[tail.size() - 1] == '/')
	{
		tail += location.index;
	}
	if(!normalizeRelativePath(tail, normalized))
	{
		return "";
	}
	return joinPath(location.root, normalized);
}

std::string PathResolver::buildFilePathFromUri(
		const ServerConfig& server,
		const std::string& uriPath) const
{
	const LocationConfig* location = matchLocation(server, uriPath);
	std::string path;

	if(location == NULL)
	{
		return "";
	}
	path = buildFilePath(*location, uriPath);
	if(path.empty() || !resolveInsideRoot(*location, path, path))
	{
		return "";
	}
	return path;
}

bool PathResolver::findCgiScript(const HttpRequest& request,
		const LocationConfig& location,
		CgiMatch& match) const
{
	std::string tail = stripLocationPrefix(location, request.path());
	std::size_t segmentEnd = 0;

	if(location.cgi.empty())
	{
		return false;
	}
	while(segmentEnd <= tail.size())
	{
		std::size_t slash = tail.find('/', segmentEnd);
		std::size_t candidateEnd = slash == std::string::npos
			? tail.size() : slash;
		std::string candidate = tail.substr(0, candidateEnd);

		for(std::map<std::string, std::string>::const_iterator it =
					location.cgi.begin(); it != location.cgi.end(); ++it)
		{
			if(!candidate.empty() && endsWith(candidate, it->first))
			{
				std::string rawScript = joinPath(location.root, candidate);
				std::string resolvedScript;
				struct stat info;

				if(!resolveInsideRoot(location, rawScript, resolvedScript))
				{
					return false;
				}
				if(stat(resolvedScript.c_str(), &info) < 0
						|| !S_ISREG(info.st_mode)
						|| access(resolvedScript.c_str(), R_OK) < 0)
				{
					return false;
				}
				match.extension = it->first;
				match.interpreter = it->second;
				match.scriptFilename = resolvedScript;
				match.scriptName = location.path == "/"
					? "/" + candidate
					: location.path + "/" + candidate;
				match.pathInfo = slash == std::string::npos
					? "" : tail.substr(slash);
				return true;
			}
		}
		if(slash == std::string::npos)
		{
			break;
		}
		segmentEnd = slash + 1;
	}
	return false;
}

bool PathResolver::endsWith(const std::string& value,
		const std::string& suffix) const
{
	return value.size() >= suffix.size()
		&& value.compare(value.size() - suffix.size(),
				suffix.size(), suffix) == 0;
}
