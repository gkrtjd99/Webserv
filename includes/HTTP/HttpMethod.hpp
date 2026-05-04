#ifndef HTTPMETHOD_HPP
# define HTTPMETHOD_HPP

#include <string>

enum HttpMethod
{
	HTTP_GET,
	HTTP_POST,
	HTTP_DELETE,
	HTTP_UNKNOWN
};

HttpMethod parseHttpMethod(const std::string& method);
const char* httpMethodToString(HttpMethod method);
bool isSupportedHttpMethod(HttpMethod method);

#endif
