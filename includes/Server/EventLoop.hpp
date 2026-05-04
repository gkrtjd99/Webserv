#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include "CgiFdRegistry.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "ListenSocketManager.hpp"
#include "PollFdBuilder.hpp"
#include "RequestDispatcher.hpp"

#include <map>
#include <signal.h>
#include <set>
#include <string>
#include <vector>

struct CgiMatch;
class HttpRequest;

class EventLoop
{
public:
	explicit EventLoop(const Config& config);

	~EventLoop();

	void run(const volatile sig_atomic_t* shutdownRequested);

private:
	std::vector<ServerConfig> _servers;
	RequestDispatcher _dispatcher;
	ListenSocketManager _listenSockets;
	PollFdBuilder _pollFds;
	CgiFdRegistry _cgiFds;
	std::map<int, Connection> _connections;

	EventLoop(const EventLoop& other);
	EventLoop& operator=(const EventLoop& other);

	void openListenSockets();
	void buildPollFds(std::vector<struct pollfd>& pollFds) const;
	void handleReadyFd(const struct pollfd& pfd);
	void handleListenFd(int fd);
	void handleClientRead(int fd);
	void handleClientWrite(int fd);
	void processParserState(int fd, Connection& connection);
	void handleCgiInput(int fd);
	void handleCgiOutput(int fd);
	void closeConnection(int fd);
	void handleCompleteRequest(int fd, Connection& connection);
	void prepareBodyLimit(Connection& connection);
	void startCgi(int fd,
			Connection& connection,
			const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			const CgiMatch& match);
	void failCgi(int fd, int status);
	bool hasActiveCgi() const;
	void checkCgiTimeouts();
};

#endif
