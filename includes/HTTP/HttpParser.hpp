#ifndef HTTPPARSER_HPP
# define HTTPPARSER_HPP

#include "HttpRequest.hpp"

#include <cstddef>
#include <string>

class HttpParser
{
public:
	enum State
	{
		READING_HEAD,
		READING_BODY,
		COMPLETE,
		FAILED,
	};

	HttpParser();

	void feed(const char* data, std::size_t len);
	void setBodyLimit(std::size_t n);
	State state() const;
	int errorStatus() const;
	const HttpRequest& request() const;
	void reset();

private:
	State _state;
	int _errorStatus;
	std::string _buffer;
	HttpRequest _request;
	std::size_t _contentLength;
	std::size_t _bodyLimit;
	bool _chunked;
	bool _hasBodyLimit;

	void parseHeadIfReady();
	void parseBodyIfReady();
	void parseStartLine(const std::string& line);
	void parseHeaderLine(const std::string& line);
	void parseHostHeader();
	void validateHttpVersion();
	void decideBodyMode();
	void parseChunkedBodyIfReady();
	void fail(int status);
};

#endif
