#include "HttpParser.hpp"
#include "HttpMethod.hpp"
#include "HttpHelper.hpp"

#include <sstream>
#include <cctype>
#include <limits>

HttpParser::HttpParser()
{
	reset();
}

void HttpParser::reset()
{
	_state = READING_HEAD;
	_errorStatus =0;
	_buffer.clear();
	_request.clear();
	_contentLength = 0;
	_bodyLimit = 0;
	_chunked = false;
	_hasBodyLimit = false;
}

void HttpParser::feed(const char* data, std::size_t len)
{
	if(_state == COMPLETE || _state == FAILED)
	{
		return;
	}

	_buffer.append(data, len);

	if(_state == READING_HEAD)
	{
		parseHeadIfReady();
	}

	if(_state == READING_BODY)
	{
		parseBodyIfReady();
	}
}

void HttpParser::parseHeadIfReady()
{
	std::size_t pos = _buffer.find("\r\n\r\n");

	if(pos == std::string::npos)
	{
		return;
	}

	std::string head = _buffer.substr(0, pos);
	_buffer = _buffer.substr(pos + 4);
	
	std::size_t lineEnd = head.find("\r\n");
	std::string requestLine;
	std::size_t begin;
	if(lineEnd == std::string::npos)
	{
		requestLine = head;
		begin = head.size();
	}
	else
	{
		requestLine = head.substr(0, lineEnd);
		begin = lineEnd + 2;
	}

	parseStartLine(requestLine);
	if(_state == FAILED)
	{
		return ;
	}

	while(begin < head.size())
	{
		lineEnd = head.find("\r\n", begin);
		std::string line;

		if(lineEnd == std::string::npos)
		{
			line = head.substr(begin);
			begin = head.size();
		}
		else
		{
			line = head.substr(begin, lineEnd - begin);
			begin = lineEnd + 2;
		}

		parseHeaderLine(line);
		if(_state == FAILED)
		{
			return;
		}
	}

	validateHttpVersion();
	if(_state == FAILED)
	{
		return;
	}

	parseHostHeader();
	if(_state == FAILED)
	{
		return;
	}

	decideBodyMode();
}

void HttpParser::parseStartLine(const std::string& line)
{
	std::istringstream iss(line);
	std::string method;
	std::string uri;
	std::string version;
	std::string extra;

	if(!(iss >> method >> uri >> version) || (iss >> extra))
	{
		fail(400);
		return;
	}

	if(uri.empty())
	{
		fail(400);
		return;
	}

	_request.setRequestLine(method, uri, version);
	if(!isSupportedHttpMethod(_request.method()))
	{
		fail(501);
	}
}

void HttpParser::parseHeaderLine(const std::string& line)
{
	std::size_t colon = line.find(':');

	if(colon == std::string::npos || colon == 0)
	{
		fail(400);
		return;
	}

	std::string name = line.substr(0, colon);
	std::string value = line.substr(colon + 1);
	std::string lowerName = HttpHelper::toLowerString(HttpHelper::trim(name));

	if((lowerName == "host"
			|| lowerName == "content-length"
			|| lowerName == "transfer-encoding")
			&& _request.hasHeader(lowerName))
	{
		fail(400);
		return;
	}

	_request.addHeader(name, value);
}

void HttpParser::parseBodyIfReady()
{
	if(!_hasBodyLimit)
	{
		return;
	}

	if(_chunked)
	{
		parseChunkedBodyIfReady();
		return;
	}

	if(_contentLength > _bodyLimit)
	{
		fail(413);
		return;
	}

	if(_buffer.size() < _contentLength)
	{
		return;
	}

	_request.setBody(_buffer.substr(0, _contentLength));
	_buffer.erase(0, _contentLength);
	_state = COMPLETE;
}

void HttpParser::parseChunkedBodyIfReady()
{
	while(_state == READING_BODY)
	{
		std::size_t lineEnd = _buffer.find("\r\n");
		if(lineEnd == std::string::npos)
		{
			return;
		}

		std::string sizeLine = _buffer.substr(0, lineEnd);
		std::size_t extension = sizeLine.find(';');
		if(extension != std::string::npos)
		{
			sizeLine = sizeLine.substr(0, extension);
		}
		sizeLine = HttpHelper::trim(sizeLine);
		if(sizeLine.empty())
		{
			fail(400);
			return;
		}

		std::size_t chunkSize = 0;
		for(std::size_t i = 0; i < sizeLine.size(); i++)
		{
			int digit = HttpHelper::hexValue(sizeLine[i]);
			if(digit < 0)
			{
				fail(400);
				return;
			}
			if(chunkSize > (std::numeric_limits<std::size_t>::max()
						- static_cast<std::size_t>(digit)) / 16)
			{
				fail(413);
				return;
			}
			chunkSize = chunkSize * 16 + static_cast<std::size_t>(digit);
		}

		std::size_t dataStart = lineEnd + 2;
		std::size_t available = _buffer.size() - dataStart;
		if(_request.body().size() > _bodyLimit
				|| chunkSize > _bodyLimit - _request.body().size())
		{
			fail(413);
			return;
		}
		if(chunkSize > available)
		{
			return;
		}

		if(chunkSize == 0)
		{
			if(available < 2)
			{
				return;
			}
			if(_buffer.compare(dataStart, 2, "\r\n") == 0)
			{
				_buffer.erase(0, dataStart + 2);
				_state = COMPLETE;
				return;
			}

			std::size_t trailerEnd = _buffer.find("\r\n\r\n", dataStart);
			if(trailerEnd == std::string::npos)
			{
				return;
			}

			std::size_t begin = dataStart;
			while(begin < trailerEnd)
			{
				std::size_t end = _buffer.find("\r\n", begin);
				if(end == std::string::npos || end > trailerEnd)
				{
					fail(400);
					return;
				}

				std::string trailerLine = _buffer.substr(begin, end - begin);
				std::size_t colon = trailerLine.find(':');

				if(colon == std::string::npos || colon == 0)
				{
					fail(400);
					return;
				}

				begin = end + 2;
			}

			_buffer.erase(0, trailerEnd + 4);
			_state = COMPLETE;
			return;
		}

		if(available - chunkSize < 2)
		{
			return;
		}
		if(_buffer[dataStart + chunkSize] != '\r'
				|| _buffer[dataStart + chunkSize + 1] != '\n')
		{
			fail(400);
			return;
		}

		_request.appendBody(_buffer.substr(dataStart, chunkSize));
		_buffer.erase(0, dataStart + chunkSize + 2);
	}
}

void HttpParser::parseHostHeader()
{
	std::string host = _request.header("host");

	if(host.empty() || HttpHelper::hasWhitespace(host))
	{
		fail(400);
		return;
	}

	std::size_t colon = host.find(':');

	if(colon == std::string::npos)
	{
		_request.setHost(host);
		return;
	}

	if(host.find(':', colon + 1) != std::string::npos)
	{
		fail(400);
		return;
	}
	
	std::string name = host.substr(0, colon);
	std::string portString = host.substr(colon + 1);

	if(name.empty() || portString.empty())
	{
		fail(400);
		return;
	}

	int port = 0;
	for(std::size_t i = 0; i < portString.size(); i++)
	{
		if(!std::isdigit(static_cast<unsigned char>(portString[i])))
		{
			fail(400);
			return;
		}

		int digit = portString[i] - '0';
		if(port > (0xffff - digit) / 10)
		{
			fail(400);
			return;
		}

		port = port * 10 + digit;
	}

	_request.setHost(name);
	_request.setHostPort(port);
}

void HttpParser::validateHttpVersion()
{
	const std::string& version = _request.version();

	if(version == "HTTP/1.1")
	{
		return;
	}
	if(version == "HTTP/1.0")
	{
		fail(505);
		return;
	}
	fail(400);
}

void HttpParser::decideBodyMode()
{
	if(_request.hasHeader("content-length") && _request.hasHeader("transfer-encoding"))
	{
		fail(400);
		return;
	}

	if(_request.hasHeader("content-length"))
	{
		const std::string& value = _request.header("content-length");

		if(value.empty())
		{
			fail(400);
			return;
		}

		_contentLength = 0;
		for(std::size_t i = 0; i < value.size(); i++)
		{
			if(!std::isdigit(static_cast<unsigned char>(value[i])))
			{
				fail(400);
				return;
			}

			std::size_t digit = static_cast<std::size_t>(value[i] - '0');
			std::size_t maxValue = std::numeric_limits<std::size_t>::max();

			if(_contentLength > (maxValue - digit) / 10)
			{
				fail(413);
				return;
			}

			_contentLength = _contentLength * 10 + digit;
		}

		if(_contentLength == 0)
		{
			_request.setBody("");
			_state = COMPLETE;
			return;
		}

		_state = READING_BODY;
		return;
	}

	if(_request.hasHeader("transfer-encoding"))
	{
		std::string value = HttpHelper::toLowerString(
				HttpHelper::trim(_request.header("transfer-encoding")));

		if(value != "chunked")
		{
			fail(400);
			return;
		}

		_chunked = true;
		_state = READING_BODY;
		return;
	}

	_state = COMPLETE;
}

void HttpParser::fail(int status)
{
	_state = FAILED;
	_errorStatus = status;
}

void HttpParser::setBodyLimit(std::size_t n)
{
	_bodyLimit = n;
	_hasBodyLimit = true;
	if(_state == READING_BODY)
	{
		parseBodyIfReady();
	}
}

HttpParser::State HttpParser::state() const
{
	return _state;
}

int HttpParser::errorStatus() const
{
	return _errorStatus;
}

const HttpRequest& HttpParser::request() const
{
	return _request;
}
