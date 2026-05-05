#ifndef LISTENSOCKETMANAGER_HPP
# define LISTENSOCKETMANAGER_HPP

#include "Config.hpp"
#include "Connection.hpp"

#include <map>
#include <poll.h>
#include <string>
#include <vector>

class ListenSocketManager
{
public:
	ListenSocketManager();
	~ListenSocketManager();

	void openAll(const std::vector<ServerConfig>& servers);
	void closeAll();
	void appendPollFds(std::vector<struct pollfd>& pollFds) const;
	bool contains(int fd) const;
	int acceptClient(int listenFd, Connection& connection) const;

private:
	std::map<int, int> _listenPorts;

	ListenSocketManager(const ListenSocketManager& other);
	ListenSocketManager& operator=(const ListenSocketManager& other);

	int openListenSocket(const std::string& host, int port) const;
	void setNonBlocking(int fd) const;
	int portFor(int fd) const;
};

#endif
