#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include "Config.hpp"
#include "Connection.hpp"

#include <map>
#include <string>
#include <vector>

class HttpRequest;

class EventLoop
{
public:
	explicit EventLoop(const ServerConfig& server);

	~EventLoop();

	void run();

private:
	ServerConfig _server;
	int _listenFd;
	std::map<int, Connection> _connections;

	EventLoop(const EventLoop& other);
	EventLoop& operator=(const EventLoop& other);

	int openListenSocket(int port);
	void setNonBlocking(int fd);
	void buildPollFds(std::vector<struct pollfd>& pollFds) const;
	void handleReadyFd(const struct pollfd& pfd);
	void handleListenFd(int fd);
	void handleClientRead(int fd);
	void handleClientWrite(int fd);
	void closeConnection(int fd);
	std::string handleRequest(const HttpRequest& request) const;
	std::string handleGet(const HttpRequest& request) const;
	std::string buildResponse(int status,
			const std::string& body,
			const std::string& contentType) const;
	std::string buildErrorResponse(int status) const;
	std::string buildFilePath(const LocationConfig& location,
			const std::string& requestPath) const;
	bool readRegularFile(const std::string& path, std::string& body) const;
	const char* statusReason(int status) const;
	const char* contentTypeForPath(const std::string& path) const;
};

#endif
