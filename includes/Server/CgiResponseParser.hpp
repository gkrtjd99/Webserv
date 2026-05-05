#ifndef CGIRESPONSEPARSER_HPP
# define CGIRESPONSEPARSER_HPP

#include "ResponseBuilder.hpp"

#include <string>

class CgiResponseParser
{
public:
	std::string buildHttpResponse(const std::string& output,
			bool keepAlive) const;

private:
	ResponseBuilder _responses;

	int parseStatusValue(const std::string& value) const;
};

#endif
