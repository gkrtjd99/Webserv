#include "EventLoop.hpp"
#include "CgiExecutor.hpp"
#include "HttpRequest.hpp"

#include <ctime>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

EventLoop::EventLoop(const Config& config)
	: _servers(config.servers),
	  _dispatcher(_servers),
	  _listenSockets(),
	  _pollFds(),
	  _cgiFds(),
	  _connections()
{
}

EventLoop::~EventLoop()
{
	for(std::map<int, Connection>::iterator it = _connections.begin();
			it != _connections.end(); ++it)
	{
		it->second.cgi().cleanup();
		close(it->first);
	}
	_listenSockets.closeAll();
}

void EventLoop::run(const volatile sig_atomic_t* shutdownRequested)
{
	std::vector<struct pollfd> pollFds;

	openListenSockets();
	while(shutdownRequested == NULL || !*shutdownRequested)
	{
		int timeout = (hasActiveCgi() || shutdownRequested != NULL) ? 1000 : -1;
		int ready;

		buildPollFds(pollFds);
		ready = poll(&pollFds[0], pollFds.size(), timeout);
		if(shutdownRequested != NULL && *shutdownRequested)
		{
			break;
		}
		if(ready < 0)
		{
			throw std::runtime_error("poll failed");
		}
		if(ready > 0)
		{
			for(std::size_t i = 0; i < pollFds.size(); ++i)
			{
				if(pollFds[i].revents != 0)
				{
					handleReadyFd(pollFds[i]);
				}
			}
		}
		checkCgiTimeouts();
	}
}

void EventLoop::openListenSockets()
{
	_listenSockets.openAll(_servers);
}

void EventLoop::buildPollFds(std::vector<struct pollfd>& pollFds) const
{
	_pollFds.build(_listenSockets, _connections, _cgiFds, pollFds);
}

void EventLoop::handleReadyFd(const struct pollfd& pfd)
{
	int clientFd;

	if(_listenSockets.contains(pfd.fd))
	{
		if((pfd.revents & POLLIN) != 0)
		{
			handleListenFd(pfd.fd);
		}
		return;
	}
	if(_cgiFds.inputOwner(pfd.fd, clientFd))
	{
		if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
		{
			failCgi(clientFd, 502);
			return;
		}
		if((pfd.revents & POLLOUT) != 0)
		{
			handleCgiInput(pfd.fd);
		}
		return;
	}
	if(_cgiFds.outputOwner(pfd.fd, clientFd))
	{
		if((pfd.revents & (POLLERR | POLLNVAL)) != 0)
		{
			failCgi(clientFd, 502);
			return;
		}
		if((pfd.revents & (POLLIN | POLLHUP)) != 0)
		{
			handleCgiOutput(pfd.fd);
		}
		return;
	}
	if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
	{
		closeConnection(pfd.fd);
		return;
	}
	if((pfd.revents & POLLIN) != 0)
	{
		handleClientRead(pfd.fd);
	}
	if(_connections.find(pfd.fd) != _connections.end()
			&& (pfd.revents & POLLOUT) != 0)
	{
		handleClientWrite(pfd.fd);
	}
}

void EventLoop::handleListenFd(int fd)
{
	Connection connection;
	int clientFd = _listenSockets.acceptClient(fd, connection);

	if(clientFd < 0)
	{
		return;
	}
	_connections[clientFd] = connection;
}

void EventLoop::handleClientRead(int fd)
{
	char buffer[4096];
	std::map<int, Connection>::iterator it = _connections.find(fd);
	ssize_t n;

	if(it == _connections.end())
	{
		return;
	}
	n = recv(fd, buffer, sizeof(buffer), 0);
	if(n > 0)
	{
		it->second.parser().feed(buffer, static_cast<std::size_t>(n));
		processParserState(fd, it->second);
		return;
	}
	closeConnection(fd);
}

void EventLoop::handleClientWrite(int fd)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);
	ssize_t n;

	if(it == _connections.end())
	{
		return;
	}
	n = send(fd, it->second.pendingWriteData(),
			it->second.pendingWriteSize(), 0);
	if(n > 0)
	{
		it->second.consumeWritten(static_cast<std::size_t>(n));
		if(it->second.writeComplete())
		{
			if(it->second.closeAfterWrite())
			{
				closeConnection(fd);
				return;
			}
			it->second.resetForNextRequest();
			processParserState(fd, it->second);
		}
		return;
	}
	closeConnection(fd);
}

void EventLoop::processParserState(int fd, Connection& connection)
{
	if(connection.parser().state() == HttpParser::READING_BODY)
	{
		prepareBodyLimit(connection);
	}
	if(connection.parser().state() == HttpParser::FAILED)
	{
		connection.setWriteBuffer(_dispatcher.buildParserError(connection,
					connection.parser().errorStatus()), true);
		return;
	}
	if(connection.parser().state() == HttpParser::COMPLETE)
	{
		handleCompleteRequest(fd, connection);
	}
}

void EventLoop::handleCgiInput(int fd)
{
	int clientFd;
	std::map<int, Connection>::iterator connection;

	if(!_cgiFds.inputOwner(fd, clientFd))
	{
		return;
	}
	connection = _connections.find(clientFd);
	if(connection == _connections.end())
	{
		_cgiFds.removeInputFd(fd);
		return;
	}
	if(!connection->second.cgi().writeInput())
	{
		failCgi(clientFd, connection->second.cgi().errorStatus());
		return;
	}
	if(!connection->second.cgi().wantsInputWrite())
	{
		_cgiFds.removeInputFd(fd);
	}
}

void EventLoop::handleCgiOutput(int fd)
{
	int clientFd;
	std::map<int, Connection>::iterator connection;

	if(!_cgiFds.outputOwner(fd, clientFd))
	{
		return;
	}
	connection = _connections.find(clientFd);
	if(connection == _connections.end())
	{
		_cgiFds.removeOutputFd(fd);
		return;
	}
	if(!connection->second.cgi().readOutput())
	{
		failCgi(clientFd, connection->second.cgi().errorStatus());
		return;
	}
	if(!connection->second.cgi().wantsOutputRead())
	{
		_cgiFds.removeOutputFd(fd);
	}
	if(connection->second.cgi().state() == CgiExecutor::FINISHED)
	{
		bool closeAfterWrite = connection->second.closeAfterWrite();
		std::string response =
			connection->second.cgi().buildHttpResponse(!closeAfterWrite);

		_cgiFds.unregisterClient(clientFd);
		connection->second.cgi().cleanup();
		connection->second.setWriteBuffer(response, closeAfterWrite);
	}
}

void EventLoop::closeConnection(int fd)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);

	if(it != _connections.end())
	{
		_cgiFds.unregisterClient(fd);
		it->second.cgi().cleanup();
		close(fd);
		_connections.erase(it);
	}
}

void EventLoop::handleCompleteRequest(int fd, Connection& connection)
{
	const HttpRequest& request = connection.parser().request();
	RequestDispatcher::Result result = _dispatcher.dispatch(connection,
			request);

	if(result.action == RequestDispatcher::START_CGI)
	{
		connection.setCloseAfterWrite(result.closeAfterWrite);
		startCgi(fd, connection, request, *result.server, *result.location,
				result.cgiMatch);
		return;
	}
	connection.setWriteBuffer(result.response, result.closeAfterWrite);
}

void EventLoop::prepareBodyLimit(Connection& connection)
{
	_dispatcher.prepareBodyLimit(connection);
}

void EventLoop::startCgi(int fd,
		Connection& connection,
		const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		const CgiMatch& match)
{
	if(!connection.cgi().start(request, server, location, match,
				connection.remoteAddress()))
	{
		connection.setWriteBuffer(_dispatcher.buildCgiError(connection,
					connection.cgi().errorStatus()),
				connection.closeAfterWrite());
		return;
	}
	connection.setCgiRunning();
	_cgiFds.registerFds(fd, connection.cgi());
}

void EventLoop::failCgi(int fd, int status)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);

	if(it == _connections.end())
	{
		return;
	}
	_cgiFds.unregisterClient(fd);
	it->second.cgi().cleanup();
	it->second.setWriteBuffer(_dispatcher.buildCgiError(it->second, status),
			it->second.closeAfterWrite());
}

bool EventLoop::hasActiveCgi() const
{
	for(std::map<int, Connection>::const_iterator it = _connections.begin();
			it != _connections.end(); ++it)
	{
		if(it->second.state() == Connection::CGI)
		{
			return true;
		}
	}
	return false;
}

void EventLoop::checkCgiTimeouts()
{
	std::time_t now = std::time(NULL);
	std::vector<int> timedOutClients;

	for(std::map<int, Connection>::iterator it = _connections.begin();
			it != _connections.end(); ++it)
	{
		if(it->second.cgi().timedOut(now))
		{
			timedOutClients.push_back(it->first);
		}
	}
	for(std::size_t i = 0; i < timedOutClients.size(); i++)
	{
		failCgi(timedOutClients[i], 504);
	}
}
