#include "CgiResponseParser.hpp"
#include "HttpHelper.hpp"

#include <sstream>

std::string CgiResponseParser::buildHttpResponse(
		const std::string& output,
		bool keepAlive) const
{
	std::size_t delimiter = output.find("\r\n\r\n");
	std::size_t delimiterLength = 4;
	std::string headerBlock;
	std::string body;
	std::istringstream lines;
	std::string line;
	std::string headers;
	int status = 200;
	bool hasLocation = false;

	if(delimiter == std::string::npos)
	{
		delimiter = output.find("\n\n");
		delimiterLength = 2;
	}
	if(delimiter == std::string::npos)
	{
		return _responses.buildError(502, NULL, keepAlive);
	}
	headerBlock = output.substr(0, delimiter);
	body = output.substr(delimiter + delimiterLength);
	lines.str(headerBlock);
	while(std::getline(lines, line))
	{
		std::size_t colon;
		std::string name;
		std::string value;
		std::string lowerName;

		if(!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);
		}
		if(line.empty())
		{
			continue;
		}
		colon = line.find(':');
		if(colon == std::string::npos || colon == 0)
		{
			return _responses.buildError(502, NULL, keepAlive);
		}
		name = line.substr(0, colon);
		value = HttpHelper::trim(line.substr(colon + 1));
		lowerName = HttpHelper::toLowerString(name);
		if(lowerName == "status")
		{
			int parsedStatus = parseStatusValue(value);

			if(parsedStatus < 100 || parsedStatus > 599)
			{
				return _responses.buildError(502, NULL, keepAlive);
			}
			status = parsedStatus;
			continue;
		}
		if(lowerName == "content-length" || lowerName == "connection")
		{
			continue;
		}
		if(lowerName == "location")
		{
			hasLocation = true;
		}
		headers += name + ": " + value + "\r\n";
	}
	if(hasLocation && status == 200)
	{
		status = 302;
	}
	std::ostringstream response;
	response << "HTTP/1.1 " << status << " "
		<< _responses.statusReason(status) << "\r\n";
	response << "Connection: " << (keepAlive ? "keep-alive" : "close")
		<< "\r\n";
	response << headers;
	response << "Content-Length: " << body.size() << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
}

int CgiResponseParser::parseStatusValue(const std::string& value) const
{
	std::string trimmed = HttpHelper::trim(value);

	if(trimmed.size() < 3)
	{
		return 0;
	}
	if(trimmed[0] < '0' || trimmed[0] > '9'
			|| trimmed[1] < '0' || trimmed[1] > '9'
			|| trimmed[2] < '0' || trimmed[2] > '9')
	{
		return 0;
	}
	return (trimmed[0] - '0') * 100
		+ (trimmed[1] - '0') * 10
		+ (trimmed[2] - '0');
}
