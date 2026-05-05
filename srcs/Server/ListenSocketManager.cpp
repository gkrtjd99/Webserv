#include "ListenSocketManager.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	std::string toString(int value)
	{
		std::ostringstream oss;

		oss << value;
		return oss.str();
	}

	std::string toStringUnsigned(unsigned long value)
	{
		std::ostringstream oss;

		oss << value;
		return oss.str();
	}

	bool parseIPv4(const std::string& host, unsigned long& value)
	{
		unsigned long result = 0;
		std::size_t begin = 0;
		int parts = 0;

		while(parts < 4)
		{
			std::size_t dot = host.find('.', begin);
			std::size_t end = dot == std::string::npos ? host.size() : dot;
			unsigned long octet = 0;

			if(end == begin)
			{
				return false;
			}
			for(std::size_t i = begin; i < end; i++)
			{
				if(host[i] < '0' || host[i] > '9')
				{
					return false;
				}
				octet = octet * 10
					+ static_cast<unsigned long>(host[i] - '0');
				if(octet > 255)
				{
					return false;
				}
			}
			result = (result << 8) | octet;
			parts++;
			if(dot == std::string::npos)
			{
				break;
			}
			begin = dot + 1;
		}
		if(parts != 4)
		{
			return false;
		}
		value = result;
		return true;
	}

	unsigned long bindAddressForHost(const std::string& host)
	{
		unsigned long value;

		if(host.empty() || host == "0.0.0.0")
		{
			return htonl(INADDR_ANY);
		}
		if(host == "localhost")
		{
			return htonl(0x7f000001UL);
		}
		if(parseIPv4(host, value))
		{
			return htonl(value);
		}
		return htonl(INADDR_ANY);
	}

	std::string formatIPv4(unsigned long hostOrderAddress)
	{
		std::string result;

		result += toStringUnsigned((hostOrderAddress >> 24) & 0xffUL);
		result += ".";
		result += toStringUnsigned((hostOrderAddress >> 16) & 0xffUL);
		result += ".";
		result += toStringUnsigned((hostOrderAddress >> 8) & 0xffUL);
		result += ".";
		result += toStringUnsigned(hostOrderAddress & 0xffUL);
		return result;
	}

	std::string listenKey(const ServerConfig& server)
	{
		return server.host + ":" + toString(server.port);
	}
}

ListenSocketManager::ListenSocketManager()
	: _listenPorts()
{
}

ListenSocketManager::~ListenSocketManager()
{
	closeAll();
}

void ListenSocketManager::openAll(const std::vector<ServerConfig>& servers)
{
	std::set<std::string> opened;

	if(servers.empty())
	{
		throw std::runtime_error("no server configuration");
	}
	for(std::size_t i = 0; i < servers.size(); i++)
	{
		std::string key = listenKey(servers[i]);

		if(opened.find(key) != opened.end())
		{
			continue;
		}
		int fd = openListenSocket(servers[i].host, servers[i].port);
		_listenPorts[fd] = servers[i].port;
		opened.insert(key);
	}
}

void ListenSocketManager::closeAll()
{
	for(std::map<int, int>::iterator it = _listenPorts.begin();
			it != _listenPorts.end(); ++it)
	{
		close(it->first);
	}
	_listenPorts.clear();
}

void ListenSocketManager::appendPollFds(
		std::vector<struct pollfd>& pollFds) const
{
	struct pollfd pfd;

	for(std::map<int, int>::const_iterator it = _listenPorts.begin();
			it != _listenPorts.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
}

bool ListenSocketManager::contains(int fd) const
{
	return _listenPorts.find(fd) != _listenPorts.end();
}

int ListenSocketManager::acceptClient(int listenFd,
		Connection& connection) const
{
	struct sockaddr_in clientAddr = sockaddr_in();
	socklen_t addrLen = sizeof(clientAddr);
	int clientFd = accept(listenFd,
			reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
	int port = portFor(listenFd);

	if(clientFd < 0 || port == 0)
	{
		if(clientFd >= 0)
		{
			close(clientFd);
		}
		return -1;
	}
	setNonBlocking(clientFd);
	connection.setLocalPort(port);
	connection.setRemoteAddress(formatIPv4(ntohl(clientAddr.sin_addr.s_addr)));
	return clientFd;
}

int ListenSocketManager::openListenSocket(const std::string& host,
		int port) const
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;
	struct sockaddr_in bindAddr = sockaddr_in();

	if(fd < 0)
	{
		throw std::runtime_error("socket failed");
	}
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		close(fd);
		throw std::runtime_error("setsockopt failed");
	}
	setNonBlocking(fd);
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = bindAddressForHost(host);
	bindAddr.sin_port = htons(static_cast<unsigned short>(port));
	if(bind(fd, reinterpret_cast<struct sockaddr*>(&bindAddr),
				sizeof(bindAddr)) < 0)
	{
		close(fd);
		throw std::runtime_error("bind failed");
	}
	if(listen(fd, 128) < 0)
	{
		close(fd);
		throw std::runtime_error("listen failed");
	}
	return fd;
}

void ListenSocketManager::setNonBlocking(int fd) const
{
	int flags = fcntl(fd, F_GETFL, 0);

	if(flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		close(fd);
		throw std::runtime_error("fcntl failed");
	}
}

int ListenSocketManager::portFor(int fd) const
{
	std::map<int, int>::const_iterator it = _listenPorts.find(fd);

	if(it == _listenPorts.end())
	{
		return 0;
	}
	return it->second;
}
