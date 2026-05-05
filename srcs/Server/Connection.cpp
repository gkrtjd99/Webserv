#include "Connection.hpp"

Connection::Connection()
	: _state(READING),
	  _cgi(),
	  _writeOffset(0),
	  _server(NULL),
	  _location(NULL),
	  _bodyLimitSet(false),
	  _localPort(0),
	  _remoteAddress(),
	  _closeAfterWrite(true)
{
}

Connection::State Connection::state() const
{
	return _state;
}

HttpParser& Connection::parser()
{
	return _parser;
}

CgiExecutor& Connection::cgi()
{
	return _cgi;
}

const CgiExecutor& Connection::cgi() const
{
	return _cgi;
}

void Connection::setWriteBuffer(const std::string& data, bool closeAfterWrite)
{
	_writeBuffer = data;
	_writeOffset = 0;
	_closeAfterWrite = closeAfterWrite;
	_state = WRITING;
}

void Connection::setCgiRunning()
{
	_state = CGI;
}

void Connection::resetForNextRequest()
{
	_parser.resetPreservingBuffer();
	_cgi.cleanup();
	_writeBuffer.clear();
	_writeOffset = 0;
	_location = NULL;
	_bodyLimitSet = false;
	_closeAfterWrite = true;
	_state = READING;
}

const char* Connection::pendingWriteData() const
{
	return _writeBuffer.data() + _writeOffset;
}

std::size_t Connection::pendingWriteSize() const
{
	return _writeBuffer.size() - _writeOffset;
}

void Connection::consumeWritten(std::size_t size)
{
	_writeOffset += size;
	if (_writeOffset > _writeBuffer.size())
	{
		_writeOffset = _writeBuffer.size();
	}
}

bool Connection::writeComplete() const
{
	return _writeOffset >= _writeBuffer.size();
}

void Connection::setServer(const ServerConfig* server)
{
	_server = server;
}

const ServerConfig* Connection::server() const
{
	return _server;
}

void Connection::setLocation(const LocationConfig* location)
{
	_location = location;
}

const LocationConfig* Connection::location() const
{
	return _location;
}

bool Connection::hasBodyLimit() const
{
	return _bodyLimitSet;
}

void Connection::markBodyLimitSet()
{
	_bodyLimitSet = true;
}

void Connection::setLocalPort(int port)
{
	_localPort = port;
}

int Connection::localPort() const
{
	return _localPort;
}

void Connection::setRemoteAddress(const std::string& remoteAddress)
{
	_remoteAddress = remoteAddress;
}

const std::string& Connection::remoteAddress() const
{
	return _remoteAddress;
}

void Connection::setCloseAfterWrite(bool closeAfterWrite)
{
	_closeAfterWrite = closeAfterWrite;
}

bool Connection::closeAfterWrite() const
{
	return _closeAfterWrite;
}
