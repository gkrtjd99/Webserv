#include "ResponseBuilder.hpp"
#include "FileResource.hpp"
#include "HttpMethod.hpp"
#include "PathResolver.hpp"

#include <map>
#include <sstream>

std::string ResponseBuilder::build(int status,
		const std::string& body,
		const std::string& contentType,
		bool keepAlive) const
{
	return buildWithHeaders(status, body, contentType, "", keepAlive);
}

std::string ResponseBuilder::buildWithHeaders(int status,
		const std::string& body,
		const std::string& contentType,
		const std::string& extraHeaders,
		bool keepAlive) const
{
	std::ostringstream response;
	std::ostringstream length;

	length << body.size();
	response << "HTTP/1.1 " << status << " " << statusReason(status) << "\r\n";
	response << "Connection: " << (keepAlive ? "keep-alive" : "close")
		<< "\r\n";
	response << extraHeaders;
	response << "Content-Length: " << length.str() << "\r\n";
	if(status != 204)
	{
		response << "Content-Type: " << contentType << "\r\n";
	}
	response << "\r\n";
	if(status != 204)
	{
		response << body;
	}
	return response.str();
}

std::string ResponseBuilder::buildError(int status,
		const ServerConfig* server,
		bool keepAlive) const
{
	std::string body = statusReason(status);

	if(server != NULL)
	{
		std::map<int, std::string>::const_iterator it =
			server->errorPages.find(status);

		if(it != server->errorPages.end())
		{
			PathResolver paths;
			FileResource files;
			std::string path = paths.buildFilePathFromUri(*server, it->second);

			if(!path.empty() && files.readRegularFile(path, body))
			{
				return build(status, body, contentTypeForPath(path), keepAlive);
			}
		}
	}
	body += "\n";
	return build(status, body, "text/plain", keepAlive);
}

std::string ResponseBuilder::buildMethodNotAllowed(
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string body = statusReason(405);
	std::string headers = "Allow: " + methodSetToAllowHeader(location.methods)
		+ "\r\n";

	body += "\n";
	return buildWithHeaders(405, body, "text/plain", headers, keepAlive);
}

std::string ResponseBuilder::buildRedirect(const LocationConfig& location,
		bool keepAlive) const
{
	std::string body = statusReason(location.redirect.first);
	std::string headers = "Location: " + location.redirect.second + "\r\n";

	body += "\n";
	return buildWithHeaders(location.redirect.first, body, "text/plain",
			headers, keepAlive);
}

const char* ResponseBuilder::statusReason(int status) const
{
	switch(status)
	{
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 204:
		return "No Content";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 303:
		return "See Other";
	case 307:
		return "Temporary Redirect";
	case 308:
		return "Permanent Redirect";
	case 400:
		return "Bad Request";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 413:
		return "Content Too Large";
	case 414:
		return "URI Too Long";
	case 431:
		return "Request Header Fields Too Large";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 502:
		return "Bad Gateway";
	case 504:
		return "Gateway Timeout";
	case 505:
		return "HTTP Version Not Supported";
	default:
		return "Error";
	}
}

const char* ResponseBuilder::contentTypeForPath(
		const std::string& path) const
{
	std::size_t dot = path.rfind('.');
	std::string ext;

	if(dot != std::string::npos)
	{
		ext = path.substr(dot + 1);
	}
	if(ext == "html" || ext == "htm")
	{
		return "text/html";
	}
	if(ext == "css")
	{
		return "text/css";
	}
	if(ext == "js")
	{
		return "application/javascript";
	}
	if(ext == "png")
	{
		return "image/png";
	}
	if(ext == "jpg" || ext == "jpeg")
	{
		return "image/jpeg";
	}
	if(ext == "gif")
	{
		return "image/gif";
	}
	return "text/plain";
}

std::string ResponseBuilder::methodSetToAllowHeader(
		const std::set<HttpMethod>& methods) const
{
	std::string result;

	for(std::set<HttpMethod>::const_iterator it = methods.begin();
			it != methods.end(); ++it)
	{
		if(!result.empty())
		{
			result += ", ";
		}
		result += httpMethodToString(*it);
	}
	return result;
}
