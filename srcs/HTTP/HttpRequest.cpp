#include "HttpRequest.hpp"
#include "HttpMethod.hpp"

#include <cctype>

namespace
{
	std::string toLowerString(const std::string& s)
	{
		std::string result;

		result.reserve(s.size());
		for(std::size_t i = 0; i < s.size(); ++i)
		{
			result += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
		}

		return result;
	}

	std::string trim(const std::string& s)
	{
		std::size_t begin = 0;
		std::size_t end = s.size();

		while(begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
		{
			begin++;
		}

		while(end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
		{
			end--;
		}

		return s.substr(begin, end - begin);
	}
}

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
	return _headers.find(toLowerString(name)) != _headers.end();
}

const std::string& HttpRequest::header(const std::string& name) const
{
	static const std::string empty;
	std::map<std::string, std::string>::const_iterator it;

	it = _headers.find(toLowerString(name));
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

void HttpRequest::setRequestLine(const std::string& method,
					const std::string& uri,
					const std::string& version)
{
	_methodString = method;
	_method = parseHttpMethod(method);
	_uri = uri;
	_version = version;
	splitUri();
}

void HttpRequest::addHeader(const std::string& name,
				const std::string& value)
{
	_headers[toLowerString(trim(name))] = trim(value);
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
}

void HttpRequest::splitUri()
{
	std::size_t pos = _uri.find('?');

	if(pos == std::string::npos)
	{
		_path = _uri;
		_query.clear();
	}
	else
	{
		_path = _uri.substr(0, pos);
		_query = _uri.substr(pos + 1);
	}
}
