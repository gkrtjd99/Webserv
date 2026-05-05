#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include "CgiExecutor.hpp"
#include "HttpParser.hpp"

#include <cstddef>
#include <string>

struct LocationConfig;
struct ServerConfig;

class Connection
{
public:
	enum State
	{
		READING,
		CGI,
		WRITING
	};

	Connection();

	State state() const;
	HttpParser& parser();
	CgiExecutor& cgi();
	const CgiExecutor& cgi() const;
	void setWriteBuffer(const std::string& data, bool closeAfterWrite);
	void setCgiRunning();
	void resetForNextRequest();

	const char* pendingWriteData() const;
	std::size_t pendingWriteSize() const;

	void consumeWritten(std::size_t size);
	bool writeComplete() const;
	void setServer(const ServerConfig* server);
	const ServerConfig* server() const;
	void setLocation(const LocationConfig* location);
	const LocationConfig* location() const;
	bool hasBodyLimit() const;
	void markBodyLimitSet();
	void setLocalPort(int port);
	int localPort() const;
	void setRemoteAddress(const std::string& remoteAddress);
	const std::string& remoteAddress() const;
	void setCloseAfterWrite(bool closeAfterWrite);
	bool closeAfterWrite() const;

private:
	State _state;
	HttpParser _parser;
	CgiExecutor _cgi;
	std::string _writeBuffer;
	std::size_t _writeOffset;
	const ServerConfig* _server;
	const LocationConfig* _location;
	bool _bodyLimitSet;
	int _localPort;
	std::string _remoteAddress;
	bool _closeAfterWrite;
};

#endif
