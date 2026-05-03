#include "HttpParser.hpp"
#include "HttpMethod.hpp"
#include "HttpHelper.hpp"
#include "HttpSyntax.hpp"

#include <cctype>
#include <limits>

namespace
{
	const std::size_t MAX_HEADER_SECTION_SIZE = 16 * 1024;
	const std::size_t MAX_HEADER_FIELD_LINE_SIZE = 4 * 1024;
	const std::size_t MAX_HEADER_COUNT = 100;
	const std::size_t MAX_REQUEST_TARGET_SIZE = 2 * 1024;
	const std::size_t MAX_CHUNK_SIZE_LINE_SIZE = 1024;
}

HttpParser::HttpParser()
{
	reset();
}

void HttpParser::reset()
{
	resetState(true);
}

void HttpParser::resetPreservingBuffer()
{
	resetState(false);
	parseHeadIfReady();
	if(_state == READING_BODY)
	{
		parseBodyIfReady();
	}
}

void HttpParser::resetState(bool clearBuffer)
{
	_state = READING_HEAD;
	_errorStatus = HTTP_STATUS_NONE;
	if(clearBuffer)
	{
		_buffer.clear();
	}
	_request.clear();
	_contentLength = 0;
	_bodyLimit = 0;
	_headerCount = 0;
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
		if(_buffer.size() > MAX_HEADER_SECTION_SIZE)
		{
			fail(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE);
		}
		return;
	}
	if(pos + 4 > MAX_HEADER_SECTION_SIZE)
	{
		fail(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE);
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

	if(requestLine.empty())
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
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

		if(line.size() > MAX_HEADER_FIELD_LINE_SIZE)
		{
			fail(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE);
			return;
		}
		_headerCount++;
		if(_headerCount > MAX_HEADER_COUNT)
		{
			fail(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE);
			return;
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
	std::string method;
	std::string uri;
	std::string version;
	std::size_t firstSpace = line.find(' ');
	std::size_t secondSpace;

	if(line.find('\t') != std::string::npos
			|| firstSpace == std::string::npos
			|| firstSpace == 0)
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	secondSpace = line.find(' ', firstSpace + 1);
	if(secondSpace == std::string::npos
			|| secondSpace == firstSpace + 1
			|| line.find(' ', secondSpace + 1) != std::string::npos
			|| secondSpace + 1 >= line.size())
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	method = line.substr(0, firstSpace);
	uri = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	version = line.substr(secondSpace + 1);

	if(!HttpSyntax::isToken(method))
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	if(uri.size() > MAX_REQUEST_TARGET_SIZE)
	{
		fail(HTTP_STATUS_URI_TOO_LONG);
		return;
	}

	HttpStatus requestLineStatus = _request.setRequestLine(method, uri, version);
	if(requestLineStatus != HTTP_STATUS_NONE)
	{
		fail(requestLineStatus);
		return;
	}

	if(!isSupportedHttpMethod(_request.method()))
	{
		fail(HTTP_STATUS_NOT_IMPLEMENTED);
	}
}

void HttpParser::parseHeaderLine(const std::string& line)
{
	std::string name;
	std::string value;

	if(!HttpSyntax::splitFieldLine(line, name, value))
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	std::string lowerName = HttpHelper::toLowerString(name);
	std::string trimmedValue = HttpHelper::trim(value);

	if((lowerName == "host" || lowerName == "transfer-encoding")
			&& _request.hasHeader(lowerName))
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	if(lowerName == "content-length" && _request.hasHeader(lowerName))
	{
		if(_request.header(lowerName) != trimmedValue)
		{
			fail(HTTP_STATUS_BAD_REQUEST);
		}
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
		fail(HTTP_STATUS_CONTENT_TOO_LARGE);
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
			if(_buffer.size() > MAX_CHUNK_SIZE_LINE_SIZE)
			{
				fail(HTTP_STATUS_BAD_REQUEST);
			}
			return;
		}

		if(lineEnd > MAX_CHUNK_SIZE_LINE_SIZE)
		{
			fail(HTTP_STATUS_BAD_REQUEST);
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
			fail(HTTP_STATUS_BAD_REQUEST);
			return;
		}

		std::size_t chunkSize = 0;
		for(std::size_t i = 0; i < sizeLine.size(); i++)
		{
			int digit = HttpSyntax::hexValue(sizeLine[i]);
			if(digit < 0)
			{
				fail(HTTP_STATUS_BAD_REQUEST);
				return;
			}
			if(chunkSize > (std::numeric_limits<std::size_t>::max()
						- static_cast<std::size_t>(digit)) / 16)
			{
				fail(HTTP_STATUS_CONTENT_TOO_LARGE);
				return;
			}
			chunkSize = chunkSize * 16 + static_cast<std::size_t>(digit);
		}

		std::size_t dataStart = lineEnd + 2;
		std::size_t available = _buffer.size() - dataStart;
		if(_request.body().size() > _bodyLimit
				|| chunkSize > _bodyLimit - _request.body().size())
		{
			fail(HTTP_STATUS_CONTENT_TOO_LARGE);
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
				std::string trailerLine;
				std::string trailerName;
				std::string trailerValue;

				if(end == std::string::npos || end > trailerEnd)
				{
					fail(HTTP_STATUS_BAD_REQUEST);
					return;
				}

				if(end - begin > MAX_HEADER_FIELD_LINE_SIZE)
				{
					fail(HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE);
					return;
				}

				trailerLine = _buffer.substr(begin, end - begin);
				if(!HttpSyntax::splitFieldLine(trailerLine, trailerName,
							trailerValue))
				{
					fail(HTTP_STATUS_BAD_REQUEST);
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
			fail(HTTP_STATUS_BAD_REQUEST);
			return;
		}

		_request.appendBody(_buffer.substr(dataStart, chunkSize));
		_buffer.erase(0, dataStart + chunkSize + 2);
	}
}

void HttpParser::parseHostHeader()
{
	std::string host = _request.header("host");

	if(host.empty() || HttpSyntax::hasWhitespace(host))
	{
		fail(HTTP_STATUS_BAD_REQUEST);
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
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}
	
	std::string name = host.substr(0, colon);
	std::string portString = host.substr(colon + 1);

	if(name.empty() || portString.empty())
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	int port = 0;
	for(std::size_t i = 0; i < portString.size(); i++)
	{
		if(!std::isdigit(static_cast<unsigned char>(portString[i])))
		{
			fail(HTTP_STATUS_BAD_REQUEST);
			return;
		}

		int digit = portString[i] - '0';
		if(port > (0xffff - digit) / 10)
		{
			fail(HTTP_STATUS_BAD_REQUEST);
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
		fail(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
		return;
	}
	if(HttpSyntax::isHttpVersion(version))
	{
		fail(HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED);
		return;
	}
	fail(HTTP_STATUS_BAD_REQUEST);
}

void HttpParser::decideBodyMode()
{
	if(_request.hasHeader("content-length") && _request.hasHeader("transfer-encoding"))
	{
		fail(HTTP_STATUS_BAD_REQUEST);
		return;
	}

	if(_request.hasHeader("content-length"))
	{
		const std::string& value = _request.header("content-length");

		if(value.empty())
		{
			fail(HTTP_STATUS_BAD_REQUEST);
			return;
		}

		_contentLength = 0;
		for(std::size_t i = 0; i < value.size(); i++)
		{
			if(!std::isdigit(static_cast<unsigned char>(value[i])))
			{
				fail(HTTP_STATUS_BAD_REQUEST);
				return;
			}

			std::size_t digit = static_cast<std::size_t>(value[i] - '0');
			std::size_t maxValue = std::numeric_limits<std::size_t>::max();

			if(_contentLength > (maxValue - digit) / 10)
			{
				fail(HTTP_STATUS_CONTENT_TOO_LARGE);
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

		if(value.empty())
		{
			fail(HTTP_STATUS_BAD_REQUEST);
			return;
		}

		if(value != "chunked")
		{
			fail(HTTP_STATUS_NOT_IMPLEMENTED);
			return;
		}

		_chunked = true;
		_state = READING_BODY;
		return;
	}

	_state = COMPLETE;
}

void HttpParser::fail(HttpStatus status)
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
	return static_cast<int>(_errorStatus);
}

const HttpRequest& HttpParser::request() const
{
	return _request;
}

const std::string& HttpParser::bufferedBytes() const
{
	return _buffer;
}
