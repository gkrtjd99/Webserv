#include "EventLoop.hpp"
#include "CgiExecutor.hpp"
#include "HttpMethod.hpp"
#include "HttpRequest.hpp"
#include "Router.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
	std::string toString(std::size_t value)
	{
		std::ostringstream oss;

		oss << value;
		return oss.str();
	}

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

	std::string toLower(const std::string& value)
	{
		std::string result;

		for(std::size_t i = 0; i < value.size(); i++)
		{
			char c = value[i];

			if(c >= 'A' && c <= 'Z')
			{
				c = static_cast<char>(c - 'A' + 'a');
			}
			result += c;
		}
		return result;
	}

	std::string trim(const std::string& value)
	{
		std::size_t begin = 0;
		std::size_t end = value.size();

		while(begin < value.size()
				&& (value[begin] == ' ' || value[begin] == '\t'))
		{
			begin++;
		}
		while(end > begin
				&& (value[end - 1] == ' ' || value[end - 1] == '\t'
					|| value[end - 1] == '\r' || value[end - 1] == '\n'))
		{
			end--;
		}
		return value.substr(begin, end - begin);
	}

	std::string joinPath(const std::string& left, const std::string& right)
	{
		if(left.empty())
		{
			return right;
		}
		if(right.empty())
		{
			return left;
		}
		if(left[left.size() - 1] == '/')
		{
			if(right[0] == '/')
			{
				return left + right.substr(1);
			}
			return left + right;
		}
		if(right[0] == '/')
		{
			return left + right;
		}
		return left + "/" + right;
	}

	bool endsWith(const std::string& value, const std::string& suffix)
	{
		return value.size() >= suffix.size()
			&& value.compare(value.size() - suffix.size(),
					suffix.size(), suffix) == 0;
	}

	bool normalizeRelativePath(const std::string& input, std::string& output)
	{
		std::vector<std::string> parts;
		std::size_t begin = 0;

		while(begin <= input.size())
		{
			std::size_t slash = input.find('/', begin);
			std::size_t end = slash == std::string::npos
				? input.size() : slash;
			std::string part = input.substr(begin, end - begin);

			if(part.empty() || part == ".")
			{
			}
			else if(part == "..")
			{
				if(parts.empty())
				{
					return false;
				}
				parts.pop_back();
			}
			else
			{
				parts.push_back(part);
			}
			if(slash == std::string::npos)
			{
				break;
			}
			begin = slash + 1;
		}
		output.clear();
		for(std::size_t i = 0; i < parts.size(); i++)
		{
			if(i != 0)
			{
				output += "/";
			}
			output += parts[i];
		}
		return true;
	}

	std::string stripLocationPrefix(const LocationConfig& location,
			const std::string& requestPath)
	{
		std::string tail = requestPath;

		if(location.path != "/" && requestPath.compare(0,
					location.path.size(), location.path) == 0)
		{
			tail = requestPath.substr(location.path.size());
		}
		if(!tail.empty() && tail[0] == '/')
		{
			tail.erase(0, 1);
		}
		return tail;
	}

	std::string htmlEscape(const std::string& value)
	{
		std::string result;

		for(std::size_t i = 0; i < value.size(); i++)
		{
			if(value[i] == '&')
				result += "&amp;";
			else if(value[i] == '<')
				result += "&lt;";
			else if(value[i] == '>')
				result += "&gt;";
			else if(value[i] == '"')
				result += "&quot;";
			else
				result += value[i];
		}
		return result;
	}

	std::string methodSetToAllowHeader(const std::set<HttpMethod>& methods)
	{
		std::string result;

		for(std::set<HttpMethod>::const_iterator it = methods.begin();
				it != methods.end(); ++it)
		{
			if(!result.empty())
			{
				result += ", ";
			}
			result += httpMethodToString(*it);
		}
		return result;
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
				octet = octet * 10 + static_cast<unsigned long>(host[i] - '0');
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

EventLoop::EventLoop(const Config& config)
	: _servers(config.servers),
	  _listenPorts(),
	  _connections(),
	  _cgiInputToClient(),
	  _cgiOutputToClient(),
	  _uploadSequence(0)
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
	for(std::map<int, int>::iterator it = _listenPorts.begin();
			it != _listenPorts.end(); ++it)
	{
		close(it->first);
	}
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
	std::set<std::string> opened;

	if(_servers.empty())
	{
		throw std::runtime_error("no server configuration");
	}
	for(std::size_t i = 0; i < _servers.size(); i++)
	{
		std::string key = listenKey(_servers[i]);

		if(opened.find(key) != opened.end())
		{
			continue;
		}
		int fd = openListenSocket(_servers[i].host, _servers[i].port);
		_listenPorts[fd] = _servers[i].port;
		opened.insert(key);
	}
}

int EventLoop::openListenSocket(const std::string& host, int port)
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

void EventLoop::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);

	if(flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		close(fd);
		throw std::runtime_error("fcntl failed");
	}
}

void EventLoop::buildPollFds(std::vector<struct pollfd>& pollFds) const
{
	struct pollfd pfd;

	pollFds.clear();
	for(std::map<int, int>::const_iterator it = _listenPorts.begin();
			it != _listenPorts.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
	for(std::map<int, Connection>::const_iterator it = _connections.begin();
			it != _connections.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = 0;
		if(it->second.state() == Connection::READING)
		{
			pfd.events |= POLLIN;
		}
		if(it->second.state() == Connection::WRITING)
		{
			pfd.events |= POLLOUT;
		}
		if(pfd.events != 0)
		{
			pfd.revents = 0;
			pollFds.push_back(pfd);
		}
	}
	for(std::map<int, int>::const_iterator it = _cgiInputToClient.begin();
			it != _cgiInputToClient.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLOUT;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
	for(std::map<int, int>::const_iterator it = _cgiOutputToClient.begin();
			it != _cgiOutputToClient.end(); ++it)
	{
		pfd.fd = it->first;
		pfd.events = POLLIN;
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
}

void EventLoop::handleReadyFd(const struct pollfd& pfd)
{
	std::map<int, int>::iterator listen = _listenPorts.find(pfd.fd);
	std::map<int, int>::iterator cgiInput = _cgiInputToClient.find(pfd.fd);
	std::map<int, int>::iterator cgiOutput = _cgiOutputToClient.find(pfd.fd);

	if(listen != _listenPorts.end())
	{
		if((pfd.revents & POLLIN) != 0)
		{
			handleListenFd(pfd.fd);
		}
		return;
	}
	if(cgiInput != _cgiInputToClient.end())
	{
		if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
		{
			failCgi(cgiInput->second, 502);
			return;
		}
		if((pfd.revents & POLLOUT) != 0)
		{
			handleCgiInput(pfd.fd);
		}
		return;
	}
	if(cgiOutput != _cgiOutputToClient.end())
	{
		if((pfd.revents & (POLLERR | POLLNVAL)) != 0)
		{
			failCgi(cgiOutput->second, 502);
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
	struct sockaddr_in clientAddr = sockaddr_in();
	socklen_t addrLen = sizeof(clientAddr);
	int clientFd = accept(fd, reinterpret_cast<struct sockaddr*>(&clientAddr),
			&addrLen);
	std::map<int, int>::iterator listen = _listenPorts.find(fd);

	if(clientFd < 0 || listen == _listenPorts.end())
	{
		return;
	}
	setNonBlocking(clientFd);
	Connection connection;
	connection.setLocalPort(listen->second);
	connection.setRemoteAddress(formatIPv4(ntohl(clientAddr.sin_addr.s_addr)));
	connection.setServer(defaultServerForPort(listen->second));
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
		const ServerConfig* server = connection.server();

		if(server == NULL)
		{
			server = defaultServerForPort(connection.localPort());
		}
		connection.setWriteBuffer(
				buildErrorResponse(connection.parser().errorStatus(),
					server, false), true);
		return;
	}
	if(connection.parser().state() == HttpParser::COMPLETE)
	{
		handleCompleteRequest(fd, connection);
	}
}

void EventLoop::handleCgiInput(int fd)
{
	std::map<int, int>::iterator owner = _cgiInputToClient.find(fd);
	std::map<int, Connection>::iterator connection;

	if(owner == _cgiInputToClient.end())
	{
		return;
	}
	connection = _connections.find(owner->second);
	if(connection == _connections.end())
	{
		_cgiInputToClient.erase(owner);
		return;
	}
	if(!connection->second.cgi().writeInput())
	{
		failCgi(owner->second, connection->second.cgi().errorStatus());
		return;
	}
	if(!connection->second.cgi().wantsInputWrite())
	{
		_cgiInputToClient.erase(fd);
	}
}

void EventLoop::handleCgiOutput(int fd)
{
	std::map<int, int>::iterator owner = _cgiOutputToClient.find(fd);
	std::map<int, Connection>::iterator connection;
	int clientFd;

	if(owner == _cgiOutputToClient.end())
	{
		return;
	}
	clientFd = owner->second;
	connection = _connections.find(clientFd);
	if(connection == _connections.end())
	{
		_cgiOutputToClient.erase(owner);
		return;
	}
	if(!connection->second.cgi().readOutput())
	{
		failCgi(clientFd, connection->second.cgi().errorStatus());
		return;
	}
	if(!connection->second.cgi().wantsOutputRead())
	{
		_cgiOutputToClient.erase(fd);
	}
	if(connection->second.cgi().state() == CgiExecutor::FINISHED)
	{
		bool closeAfterWrite = connection->second.closeAfterWrite();
		std::string response =
			connection->second.cgi().buildHttpResponse(!closeAfterWrite);

		unregisterCgiFds(clientFd);
		connection->second.cgi().cleanup();
		connection->second.setWriteBuffer(response, closeAfterWrite);
	}
}

void EventLoop::closeConnection(int fd)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);

	if(it != _connections.end())
	{
		unregisterCgiFds(fd);
		it->second.cgi().cleanup();
		close(fd);
		_connections.erase(it);
	}
}

void EventLoop::handleCompleteRequest(int fd, Connection& connection)
{
	const HttpRequest& request = connection.parser().request();
	const ServerConfig* server = resolveServer(connection);
	const LocationConfig* location = resolveLocation(connection);
	bool keepAlive = shouldKeepAlive(request);
	CgiMatch cgiMatch;

	if(server == NULL)
	{
		connection.setWriteBuffer(buildErrorResponse(500, NULL, false), true);
		return;
	}
	if(location == NULL)
	{
		connection.setWriteBuffer(buildErrorResponse(404, server, keepAlive),
				!keepAlive);
		return;
	}
	if(location->redirect.first != 0)
	{
		connection.setWriteBuffer(handleRedirect(*location, keepAlive),
				!keepAlive);
		return;
	}
	if(!locationAllowsMethod(*location, request.method()))
	{
		connection.setWriteBuffer(build405Response(*server, *location,
					keepAlive), !keepAlive);
		return;
	}
	if(findCgiScript(request, *location, cgiMatch))
	{
		connection.setCloseAfterWrite(!keepAlive);
		startCgi(fd, connection, request, *server, *location, cgiMatch);
		return;
	}
	connection.setWriteBuffer(handleRequest(connection, request, *server,
				*location), !keepAlive);
}

void EventLoop::prepareBodyLimit(Connection& connection)
{
	const ServerConfig* server;
	const LocationConfig* location;

	if(connection.hasBodyLimit())
	{
		return;
	}
	server = resolveServer(connection);
	location = resolveLocation(connection);
	connection.markBodyLimitSet();
	if(location != NULL)
	{
		connection.parser().setBodyLimit(location->clientMaxBodySize);
		return;
	}
	if(server != NULL)
	{
		connection.parser().setBodyLimit(server->clientMaxBodySize);
	}
}

const ServerConfig* EventLoop::resolveServer(Connection& connection)
{
	const HttpRequest& request = connection.parser().request();
	const ServerConfig* server;

	if(connection.server() != NULL
			&& request.getHost().empty())
	{
		return connection.server();
	}
	server = matchServer(_servers, request.getHost(), connection.localPort());
	if(server == NULL)
	{
		server = defaultServerForPort(connection.localPort());
	}
	connection.setServer(server);
	return server;
}

const LocationConfig* EventLoop::resolveLocation(Connection& connection)
{
	const ServerConfig* server;
	const LocationConfig* location;

	if(connection.location() != NULL)
	{
		return connection.location();
	}
	server = resolveServer(connection);
	if(server == NULL)
	{
		return NULL;
	}
	location = matchLocation(*server, connection.parser().request().path());
	connection.setLocation(location);
	return location;
}

const ServerConfig* EventLoop::defaultServerForPort(int port) const
{
	for(std::size_t i = 0; i < _servers.size(); i++)
	{
		if(_servers[i].port == port)
		{
			return &_servers[i];
		}
	}
	if(!_servers.empty())
	{
		return &_servers[0];
	}
	return NULL;
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
		connection.setWriteBuffer(
				buildErrorResponse(connection.cgi().errorStatus(), &server,
					!connection.closeAfterWrite()),
				connection.closeAfterWrite());
		return;
	}
	connection.setCgiRunning();
	registerCgiFds(fd, connection.cgi());
}

void EventLoop::failCgi(int fd, int status)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);
	const ServerConfig* server;

	if(it == _connections.end())
	{
		return;
	}
	server = it->second.server();
	unregisterCgiFds(fd);
	it->second.cgi().cleanup();
	it->second.setWriteBuffer(buildErrorResponse(status, server,
				!it->second.closeAfterWrite()), it->second.closeAfterWrite());
}

void EventLoop::registerCgiFds(int clientFd, const CgiExecutor& cgi)
{
	if(cgi.wantsInputWrite())
	{
		_cgiInputToClient[cgi.inputFd()] = clientFd;
	}
	if(cgi.wantsOutputRead())
	{
		_cgiOutputToClient[cgi.outputFd()] = clientFd;
	}
}

void EventLoop::unregisterCgiFds(int clientFd)
{
	for(std::map<int, int>::iterator it = _cgiInputToClient.begin();
			it != _cgiInputToClient.end();)
	{
		if(it->second == clientFd)
		{
			_cgiInputToClient.erase(it++);
		}
		else
		{
			++it;
		}
	}
	for(std::map<int, int>::iterator it = _cgiOutputToClient.begin();
			it != _cgiOutputToClient.end();)
	{
		if(it->second == clientFd)
		{
			_cgiOutputToClient.erase(it++);
		}
		else
		{
			++it;
		}
	}
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

std::string EventLoop::handleRequest(Connection& connection,
		const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location)
{
	bool keepAlive = shouldKeepAlive(request);

	(void)connection;
	if(request.method() == HTTP_GET)
	{
		return handleGet(request, server, location, keepAlive);
	}
	if(request.method() == HTTP_POST)
	{
		return handleUpload(request, server, location, keepAlive);
	}
	if(request.method() == HTTP_DELETE)
	{
		return handleDelete(request, server, location, keepAlive);
	}
	return buildErrorResponse(501, &server, keepAlive);
}

std::string EventLoop::handleGet(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string path;
	std::string body;
	struct stat info;
	std::string tail = stripLocationPrefix(location, request.path());
	std::string normalizedTail;

	if((tail.empty()
				|| (!request.path().empty()
					&& request.path()[request.path().size() - 1] == '/'))
			&& normalizeRelativePath(tail, normalizedTail))
	{
		std::string directoryPath = normalizedTail.empty()
			? location.root : joinPath(location.root, normalizedTail);

		if(stat(directoryPath.c_str(), &info) == 0 && S_ISDIR(info.st_mode))
		{
			path = joinPath(directoryPath, location.index);
			if(stat(path.c_str(), &info) < 0)
			{
				if(location.autoindex)
				{
						return buildAutoindex(request, server, location,
								directoryPath, keepAlive);
				}
				return buildErrorResponse(403, &server, keepAlive);
			}
		}
	}

	if(path.empty())
	{
		path = buildFilePath(location, request.path());
	}
	if(path.empty())
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(stat(path.c_str(), &info) < 0)
	{
		return buildErrorResponse(404, &server, keepAlive);
	}
	if(S_ISDIR(info.st_mode))
	{
		std::string directoryPath = path;

		path = buildFilePath(location, request.path() + "/");
		if(path.empty() || stat(path.c_str(), &info) < 0)
		{
			if(location.autoindex)
			{
					return buildAutoindex(request, server, location,
							directoryPath, keepAlive);
			}
			return buildErrorResponse(403, &server, keepAlive);
		}
	}
	if(!S_ISREG(info.st_mode))
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(!resolvePathInsideRoot(location, path, path))
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(access(path.c_str(), R_OK) < 0)
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(!readRegularFile(path, body))
	{
		return buildErrorResponse(500, &server, keepAlive);
	}
	return buildResponse(200, body, contentTypeForPath(path), keepAlive);
}

std::string EventLoop::handleUpload(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive)
{
	if(location.uploadStore.empty())
	{
		return buildErrorResponse(501, &server, keepAlive);
	}
	for(int attempt = 0; attempt < 1000; attempt++)
	{
		std::string filename = "upload-" + toString(static_cast<std::size_t>(
					std::time(NULL))) + "-" + toString(_uploadSequence++)
			+ ".bin";
		std::string path = joinPath(location.uploadStore, filename);
		int fd = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
		std::size_t offset = 0;

		if(fd < 0)
		{
			continue;
		}
		while(offset < request.body().size())
		{
			ssize_t n = write(fd, request.body().data() + offset,
					request.body().size() - offset);

				if(n <= 0)
				{
					close(fd);
					std::remove(path.c_str());
					return buildErrorResponse(500, &server, keepAlive);
				}
			offset += static_cast<std::size_t>(n);
		}
		close(fd);
		std::string body = "Created\n";
		std::string uri = location.path == "/"
			? "/" + filename : location.path + "/" + filename;
		std::string headers = "Location: " + uri + "\r\n";

		return buildResponseWithHeaders(201, body, "text/plain", headers,
				keepAlive);
	}
	return buildErrorResponse(500, &server, keepAlive);
}

std::string EventLoop::handleDelete(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string path = buildFilePath(location, request.path());
	struct stat info;

	if(path.empty())
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(stat(path.c_str(), &info) < 0)
	{
		return buildErrorResponse(404, &server, keepAlive);
	}
	if(!S_ISREG(info.st_mode))
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(!resolvePathInsideRoot(location, path, path))
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(!removeRegularFile(path))
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	return buildResponse(204, "", "text/plain", keepAlive);
}

std::string EventLoop::handleRedirect(const LocationConfig& location,
		bool keepAlive) const
{
	std::string body = statusReason(location.redirect.first);
	std::string headers = "Location: " + location.redirect.second + "\r\n";

	body += "\n";
	return buildResponseWithHeaders(location.redirect.first, body,
			"text/plain", headers, keepAlive);
}

std::string EventLoop::buildAutoindex(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		const std::string& directoryPath,
		bool keepAlive) const
{
	DIR* dir = opendir(directoryPath.c_str());
	std::string body;
	std::string base = request.path();

	if(dir == NULL)
	{
		return buildErrorResponse(403, &server, keepAlive);
	}
	if(base.empty() || base[base.size() - 1] != '/')
	{
		base += "/";
	}
	body = "<html><head><title>Index of " + htmlEscape(request.path())
		+ "</title></head><body><h1>Index of " + htmlEscape(request.path())
		+ "</h1><ul>";
	while(true)
	{
		struct dirent* entry = readdir(dir);
		std::string name;

		if(entry == NULL)
		{
			break;
		}
		name = entry->d_name;
		if(name == ".")
		{
			continue;
		}
		body += "<li><a href=\"" + htmlEscape(base + name) + "\">"
			+ htmlEscape(name) + "</a></li>";
	}
	closedir(dir);
	body += "</ul></body></html>\n";
	(void)location;
	return buildResponse(200, body, "text/html", keepAlive);
}

std::string EventLoop::build405Response(const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive) const
{
	std::string body = statusReason(405);
	std::string headers = "Allow: " + methodSetToAllowHeader(location.methods)
		+ "\r\n";

	body += "\n";
	(void)server;
	return buildResponseWithHeaders(405, body, "text/plain", headers,
			keepAlive);
}

std::string EventLoop::buildResponse(int status,
		const std::string& body,
		const std::string& contentType,
		bool keepAlive) const
{
	return buildResponseWithHeaders(status, body, contentType, "", keepAlive);
}

std::string EventLoop::buildResponseWithHeaders(int status,
		const std::string& body,
		const std::string& contentType,
		const std::string& extraHeaders,
		bool keepAlive) const
{
	std::ostringstream response;
	std::ostringstream length;

	length << body.size();
	response << "HTTP/1.1 " << status << " " << statusReason(status) << "\r\n";
	response << "Connection: " << (keepAlive ? "keep-alive" : "close")
		<< "\r\n";
	response << extraHeaders;
	response << "Content-Length: " << length.str() << "\r\n";
	if(status != 204)
	{
		response << "Content-Type: " << contentType << "\r\n";
	}
	response << "\r\n";
	if(status != 204)
	{
		response << body;
	}
	return response.str();
}

std::string EventLoop::buildErrorResponse(int status,
		const ServerConfig* server,
		bool keepAlive) const
{
	std::string body = statusReason(status);

	if(server != NULL)
	{
		std::map<int, std::string>::const_iterator it =
			server->errorPages.find(status);

		if(it != server->errorPages.end())
		{
			std::string path = buildFilePathFromUri(*server, it->second);

			if(!path.empty() && readRegularFile(path, body))
			{
				return buildResponse(status, body, contentTypeForPath(path),
						keepAlive);
			}
		}
	}
	body += "\n";
	return buildResponse(status, body, "text/plain", keepAlive);
}

bool EventLoop::locationAllowsMethod(const LocationConfig& location,
		HttpMethod method) const
{
	return location.methods.find(method) != location.methods.end();
}

bool EventLoop::shouldKeepAlive(const HttpRequest& request) const
{
	std::string connection = toLower(trim(request.header("connection")));

	return request.version() == "HTTP/1.1" && connection != "close";
}

bool EventLoop::findCgiScript(const HttpRequest& request,
		const LocationConfig& location,
		CgiMatch& match) const
{
	std::string tail = stripLocationPrefix(location, request.path());
	std::size_t segmentEnd = 0;

	if(location.cgi.empty())
	{
		return false;
	}
	while(segmentEnd <= tail.size())
	{
		std::size_t slash = tail.find('/', segmentEnd);
		std::size_t candidateEnd = slash == std::string::npos
			? tail.size() : slash;
		std::string candidate = tail.substr(0, candidateEnd);

		for(std::map<std::string, std::string>::const_iterator it =
					location.cgi.begin(); it != location.cgi.end(); ++it)
		{
			if(!candidate.empty() && endsWith(candidate, it->first))
			{
				std::string rawScript = joinPath(location.root, candidate);
				std::string resolvedScript;
				struct stat info;

				if(!resolvePathInsideRoot(location, rawScript, resolvedScript))
				{
					return false;
				}
				if(stat(resolvedScript.c_str(), &info) < 0
						|| !S_ISREG(info.st_mode)
						|| access(resolvedScript.c_str(), R_OK) < 0)
				{
					return false;
				}
				match.extension = it->first;
				match.interpreter = it->second;
				match.scriptFilename = resolvedScript;
				match.scriptName = location.path == "/"
					? "/" + candidate
					: location.path + "/" + candidate;
				match.pathInfo = slash == std::string::npos
					? "" : tail.substr(slash);
				return true;
			}
		}
		if(slash == std::string::npos)
		{
			break;
		}
		segmentEnd = slash + 1;
	}
	return false;
}

std::string EventLoop::buildFilePath(const LocationConfig& location,
		const std::string& requestPath) const
{
	std::string tail;
	std::string normalized;

	if(requestPath.empty() || requestPath[0] != '/')
	{
		return "";
	}
	tail = stripLocationPrefix(location, requestPath);
	if(tail.empty() || tail[tail.size() - 1] == '/')
	{
		tail += location.index;
	}
	if(!normalizeRelativePath(tail, normalized))
	{
		return "";
	}
	return joinPath(location.root, normalized);
}

std::string EventLoop::buildFilePathFromUri(const ServerConfig& server,
		const std::string& uriPath) const
{
	const LocationConfig* location = matchLocation(server, uriPath);
	std::string path;

	if(location == NULL)
	{
		return "";
	}
	path = buildFilePath(*location, uriPath);
	if(path.empty() || !resolvePathInsideRoot(*location, path, path))
	{
		return "";
	}
	return path;
}

bool EventLoop::resolvePathInsideRoot(const LocationConfig& location,
		const std::string& rawPath,
		std::string& resolvedPath) const
{
	std::string root = location.root;
	std::string tail;
	std::string normalized;

	if(root.empty())
	{
		return false;
	}
	if(root != "/" && rawPath == root)
	{
		resolvedPath = root;
		return true;
	}
	if(root == "/")
	{
		if(rawPath.empty() || rawPath[0] != '/')
		{
			return false;
		}
		tail = rawPath.substr(1);
	}
	else
	{
		if(rawPath.size() <= root.size()
				|| rawPath.compare(0, root.size(), root) != 0
				|| rawPath[root.size()] != '/')
		{
			return false;
		}
		tail = rawPath.substr(root.size() + 1);
	}
	if(!normalizeRelativePath(tail, normalized))
	{
		return false;
	}
	if(normalized.empty())
	{
		resolvedPath = root;
	}
	else
	{
		resolvedPath = joinPath(root, normalized);
	}
	return true;
}

bool EventLoop::readRegularFile(const std::string& path, std::string& body) const
{
	int fd = open(path.c_str(), O_RDONLY);
	char buffer[4096];

	if(fd < 0)
	{
		return false;
	}
	body.clear();
	while(true)
	{
		ssize_t n = read(fd, buffer, sizeof(buffer));

		if(n < 0)
		{
			close(fd);
			return false;
		}
		if(n == 0)
		{
			break;
		}
		body.append(buffer, static_cast<std::size_t>(n));
	}
	close(fd);
	return true;
}

bool EventLoop::removeRegularFile(const std::string& path) const
{
	return std::remove(path.c_str()) == 0;
}

const char* EventLoop::statusReason(int status) const
{
	switch(status)
	{
	case 200:
		return "OK";
	case 201:
		return "Created";
	case 204:
		return "No Content";
	case 301:
		return "Moved Permanently";
	case 302:
		return "Found";
	case 303:
		return "See Other";
	case 307:
		return "Temporary Redirect";
	case 308:
		return "Permanent Redirect";
	case 400:
		return "Bad Request";
	case 403:
		return "Forbidden";
	case 404:
		return "Not Found";
	case 405:
		return "Method Not Allowed";
	case 413:
		return "Content Too Large";
	case 414:
		return "URI Too Long";
	case 431:
		return "Request Header Fields Too Large";
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
	case 502:
		return "Bad Gateway";
	case 504:
		return "Gateway Timeout";
	case 505:
		return "HTTP Version Not Supported";
	default:
		return "Error";
	}
}

const char* EventLoop::contentTypeForPath(const std::string& path) const
{
	std::size_t dot = path.rfind('.');
	std::string ext;

	if(dot != std::string::npos)
	{
		ext = path.substr(dot + 1);
	}
	if(ext == "html" || ext == "htm")
	{
		return "text/html";
	}
	if(ext == "css")
	{
		return "text/css";
	}
	if(ext == "js")
	{
		return "application/javascript";
	}
	if(ext == "png")
	{
		return "image/png";
	}
	if(ext == "jpg" || ext == "jpeg")
	{
		return "image/jpeg";
	}
	if(ext == "gif")
	{
		return "image/gif";
	}
	return "text/plain";
}
