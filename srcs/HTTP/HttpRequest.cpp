#include "HttpRequest.hpp"
#include "HttpMethod.hpp"
#include "HttpHelper.hpp"
#include "HttpSyntax.hpp"

HttpRequest::HttpRequest()
{
	clear();
}

HttpMethod HttpRequest::method() const
{
	return _method;
}

const std::string& HttpRequest::methodString() const
{
	return _methodString;
}

const std::string& HttpRequest::uri() const
{
	return _uri;
}

const std::string& HttpRequest::path() const
{
	return _path;
}

const std::string& HttpRequest::query() const
{
	return _query;
}

const std::string& HttpRequest::version() const
{
	return _version;
}

bool HttpRequest::hasHeader(const std::string& name) const
{
	return _headers.find(HttpHelper::toLowerString(name)) != _headers.end();
}

const std::string& HttpRequest::header(const std::string& name) const
{
	static const std::string empty;
	std::map<std::string, std::string>::const_iterator it;

	it = _headers.find(HttpHelper::toLowerString(name));
	if(it == _headers.end())
	{
		return empty;
	}

	return it->second;
}

const std::map<std::string, std::string>& HttpRequest::headers() const
{
	return _headers;
}

const std::string& HttpRequest::body() const
{
	return _body;
}

bool HttpRequest::setRequestLine(const std::string& method,
					const std::string& uri,
					const std::string& version)
{
	_methodString = method;
	_method = parseHttpMethod(method);
	_uri = uri;
	_version = version;
	return splitUri();
}

void HttpRequest::addHeader(const std::string& name,
				const std::string& value)
{
	_headers[HttpHelper::toLowerString(HttpHelper::trim(name))] = HttpHelper::trim(value);
}

void HttpRequest::setBody(const std::string& body)
{
	_body = body;
}

void HttpRequest::appendBody(const std::string& data)
{
	_body += data;
}

void HttpRequest::clear()
{
	_method = HTTP_UNKNOWN;
	_methodString.clear();
	_uri.clear();
	_path.clear();
	_query.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_host.clear();
	_hostPort = 0;
	_hasHostPort = false;
}

const std::string& HttpRequest::getHost() const
{
	return _host;
}

int HttpRequest::getHostPort() const
{
	return _hostPort;
}

bool HttpRequest::hasHostPort() const
{
	return _hasHostPort;
}

void HttpRequest::setHost(const std::string& host)
{
	_host = host;
}

void HttpRequest::setHostPort(int port)
{
	_hostPort = port;
	_hasHostPort = true;
}

bool HttpRequest::splitUri()
{
	std::size_t pos = _uri.find('?');
	std::string rawPath;
	std::string decodedPath;

	if(_uri.empty() || _uri.find('#') != std::string::npos
			|| _uri[0] != '/')
	{
		_path.clear();
		_query.clear();
		return false;
	}

	if(pos == std::string::npos)
	{
		rawPath = _uri;
		_query.clear();
	}

	else
	{
		rawPath = _uri.substr(0, pos);
		_query = _uri.substr(pos + 1);
	}

	if(rawPath.empty())
	{
		rawPath = "/";
	}

	if(!HttpSyntax::percentDecodePath(rawPath, decodedPath)
			|| !HttpSyntax::normalizeDecodedPath(decodedPath, _path)
			|| !HttpSyntax::percentTripletsAreValid(_query))
	{
		_path.clear();
		_query.clear();
		return false;
	}

	return true;
}
