#include "CgiEnvironment.hpp"
#include "HttpHelper.hpp"

#include <map>
#include <sstream>

std::vector<std::string> CgiEnvironment::build(
		const HttpRequest& request,
		const ServerConfig& server,
		const CgiMatch& match,
		const std::string& remoteAddress) const
{
	std::vector<std::string> env;
	std::string contentType = request.header("content-type");
	const std::map<std::string, std::string>& headers = request.headers();

	add(env, "GATEWAY_INTERFACE", "CGI/1.1");
	add(env, "SERVER_PROTOCOL", request.version());
	add(env, "REQUEST_METHOD", request.methodString());
	add(env, "SCRIPT_FILENAME", match.scriptFilename);
	add(env, "SCRIPT_NAME", match.scriptName);
	add(env, "PATH_INFO", match.pathInfo);
	add(env, "QUERY_STRING", request.query());
	add(env, "CONTENT_LENGTH", toString(request.body().size()));
	add(env, "CONTENT_TYPE", contentType);
	add(env, "SERVER_NAME", request.getHost());
	add(env, "SERVER_PORT", toString(server.port));
	add(env, "REMOTE_ADDR", remoteAddress);
	add(env, "REDIRECT_STATUS", "200");

	for(std::map<std::string, std::string>::const_iterator it = headers.begin();
			it != headers.end(); ++it)
	{
		std::string name = HttpHelper::toLowerString(it->first);

		if(name == "content-type" || name == "content-length")
		{
			continue;
		}
		add(env, "HTTP_" + toUpperHeaderName(it->first), it->second);
	}
	return env;
}

void CgiEnvironment::buildCharArray(
		const std::vector<std::string>& strings,
		std::vector<char*>& result) const
{
	result.clear();
	for(std::size_t i = 0; i < strings.size(); i++)
	{
		result.push_back(const_cast<char*>(strings[i].c_str()));
	}
	result.push_back(NULL);
}

void CgiEnvironment::add(std::vector<std::string>& env,
		const std::string& name,
		const std::string& value) const
{
	env.push_back(name + "=" + value);
}

std::string CgiEnvironment::toString(std::size_t value) const
{
	std::ostringstream oss;

	oss << value;
	return oss.str();
}

std::string CgiEnvironment::toString(int value) const
{
	std::ostringstream oss;

	oss << value;
	return oss.str();
}

std::string CgiEnvironment::toUpperHeaderName(
		const std::string& value) const
{
	std::string result;

	for(std::size_t i = 0; i < value.size(); i++)
	{
		char c = value[i];

		if(c >= 'a' && c <= 'z')
		{
			c = static_cast<char>(c - 'a' + 'A');
		}
		else if(c == '-')
		{
			c = '_';
		}
		result += c;
	}
	return result;
}
