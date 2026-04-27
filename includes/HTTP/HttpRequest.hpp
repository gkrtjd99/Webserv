#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include "HttpMethod.hpp"

#include <string>
#include <map>

class HttpRequest
{
public:
	HttpRequest();

    HttpMethod method() const;
    const std::string& methodString() const;
    const std::string& uri() const;
    const std::string& path() const;
    const std::string& query() const;
    const std::string& version() const;

    bool hasHeader(const std::string& name) const;
    const std::string& header(const std::string& name) const;
    const std::map<std::string, std::string>& headers() const;

    const std::string& body() const;

	void setRequestLine(const std::string& method,
					const std::string& uri,
					const std::string& version);
	void addHeader(const std::string& name,
					const std::string& value);
	void setBody(const std::string& body);
	void appendBody(const std::string& data);
	void clear();

private:
	HttpMethod _method;
	std::string _methodString;
	std::string _uri;
	std::string _path;
	std::string _query;
	std::string _version;
	std::map<std::string, std::string> _headers;
	std::string _body;

	void splitUri();
};

#endif
