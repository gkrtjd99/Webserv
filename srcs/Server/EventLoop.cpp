#include "EventLoop.hpp"
#include "HttpMethod.hpp"
#include "HttpRequest.hpp"
#include "Router.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

EventLoop::EventLoop(const ServerConfig& server)
	: _server(server),
	  _listenFd(-1)
{
}

EventLoop::~EventLoop()
{
	for (std::map<int, Connection>::iterator it = _connections.begin();
		 it != _connections.end();
		 ++it)
	{
		close(it->first);
	}
	if (_listenFd != -1)
	{
		close(_listenFd);
	}
}

void EventLoop::run()
{
	std::vector<struct pollfd> pollFds;

	_listenFd = openListenSocket(_server.port);
	while (true)
	{
		buildPollFds(pollFds);
		if (poll(&pollFds[0], pollFds.size(), -1) < 0)
		{
			throw std::runtime_error("poll failed");
		}
		for (std::size_t i = 0; i < pollFds.size(); ++i)
		{
			if (pollFds[i].revents != 0)
			{
				handleReadyFd(pollFds[i]);
			}
		}
	}
}

int EventLoop::openListenSocket(int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int yes = 1;

	if (fd < 0)
	{
		throw std::runtime_error("socket failed");
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		close(fd);
		throw std::runtime_error("setsockopt failed");
	}
	setNonBlocking(fd);
	struct sockaddr_in bindAddr = sockaddr_in();
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	bindAddr.sin_port = htons(static_cast<unsigned short>(port));
	if (bind(fd, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0)
	{
		close(fd);
		throw std::runtime_error("bind failed");
	}
	if (listen(fd, 128) < 0)
	{
		close(fd);
		throw std::runtime_error("listen failed");
	}
	return fd;
}

void EventLoop::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		throw std::runtime_error("fcntl failed");
	}
}

void EventLoop::buildPollFds(std::vector<struct pollfd>& pollFds) const
{
	struct pollfd pfd;

	pollFds.clear();
	pfd.fd = _listenFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	pollFds.push_back(pfd);
	for (std::map<int, Connection>::const_iterator it = _connections.begin();
		 it != _connections.end();
		 ++it)
	{
		pfd.fd = it->first;
		pfd.events = 0;
		if (it->second.state() == Connection::READING)
		{
			pfd.events |= POLLIN;
		}
		if (it->second.state() == Connection::WRITING)
		{
			pfd.events |= POLLOUT;
		}
		pfd.revents = 0;
		pollFds.push_back(pfd);
	}
}

void EventLoop::handleReadyFd(const struct pollfd& pfd)
{
	if (pfd.fd == _listenFd)
	{
		if ((pfd.revents & POLLIN) != 0)
		{
			handleListenFd(pfd.fd);
		}
		return;
	}
	if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
	{
		closeConnection(pfd.fd);
		return;
	}
	if ((pfd.revents & POLLIN) != 0)
	{
		handleClientRead(pfd.fd);
	}
	if (_connections.find(pfd.fd) != _connections.end()
		&& (pfd.revents & POLLOUT) != 0)
	{
		handleClientWrite(pfd.fd);
	}
}

void EventLoop::handleListenFd(int fd)
{
	int clientFd = accept(fd, NULL, NULL);

	if (clientFd < 0)
	{
		return;
	}
	setNonBlocking(clientFd);
	Connection connection;

	connection.parser().setBodyLimit(_server.clientMaxBodySize);
	_connections[clientFd] = connection;
}

void EventLoop::handleClientRead(int fd)
{
	char buffer[4096];
	ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
	std::map<int, Connection>::iterator it = _connections.find(fd);

	if (it == _connections.end())
	{
		return;
	}
	if (n > 0)
	{
		/**
		 * A 확인 필요: parser 는 complete, failed, body limit 초과,
		 * chunked body 처리 결과를 COMPLETE 또는 FAILED 로 끝내줘야 한다.
		 */
		it->second.parser().feed(buffer, static_cast<std::size_t>(n));
		if (it->second.parser().state() == HttpParser::FAILED)
		{
			it->second.setWriteBuffer(
					buildErrorResponse(it->second.parser().errorStatus()));
		}
		else if (it->second.parser().state() == HttpParser::COMPLETE)
		{
			it->second.setWriteBuffer(
					handleRequest(it->second.parser().request()));
		}
		return;
	}
	closeConnection(fd);
}

void EventLoop::handleClientWrite(int fd)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);
	ssize_t n;

	if (it == _connections.end())
	{
		return;
	}
	n = send(fd, it->second.pendingWriteData(), it->second.pendingWriteSize(), 0);
	if (n > 0)
	{
		it->second.consumeWritten(static_cast<std::size_t>(n));
		if (it->second.writeComplete())
		{
			closeConnection(fd);
		}
		return;
	}
	closeConnection(fd);
}

void EventLoop::closeConnection(int fd)
{
	std::map<int, Connection>::iterator it = _connections.find(fd);

	if (it != _connections.end())
	{
		close(fd);
		_connections.erase(it);
	}
}

/**
 * A/B 확인 필요: A의 Router가 multiple server 선택까지 맡으면
 * 이 함수는 matched ServerConfig/LocationConfig 를 인자로 받는 형태로 바꾼다.
 * B는 location root/index/methods/error_page/clientMaxBodySize 값을 채워야 한다.
 */
std::string EventLoop::handleRequest(const HttpRequest& request) const
{
	if (request.method() == HTTP_GET)
		return handleGet(request);
	/**
	 * B 확인 필요: methods/upload_store 설정이 들어오면 POST/DELETE 를
	 * 여기서 고정 405 로 막지 않고 route 정책에 따라 처리한다.
	 */
	if (request.method() == HTTP_POST || request.method() == HTTP_DELETE)
		return buildErrorResponse(405);
	return buildErrorResponse(501);
}

std::string EventLoop::handleGet(const HttpRequest& request) const
{
	const LocationConfig* location = matchLocation(_server, request.path());
	std::string path;
	std::string body;
	struct stat info;

	if (location == NULL)
		return buildErrorResponse(404);
	path = buildFilePath(*location, request.path());
	if (path.empty())
		return buildErrorResponse(403);
	if (stat(path.c_str(), &info) < 0)
		return buildErrorResponse(404);
	if (S_ISDIR(info.st_mode))
	{
		path = buildFilePath(*location, request.path() + "/");
		if (stat(path.c_str(), &info) < 0)
			return buildErrorResponse(404);
	}
	if (!S_ISREG(info.st_mode))
		return buildErrorResponse(403);
	if (access(path.c_str(), R_OK) < 0)
		return buildErrorResponse(403);
	if (!readRegularFile(path, body))
		return buildErrorResponse(500);
	return buildResponse(200, body, contentTypeForPath(path));
}

std::string EventLoop::buildResponse(int status,
		const std::string& body,
		const std::string& contentType) const
{
	std::ostringstream response;
	std::ostringstream length;

	length << body.size();
	response << "HTTP/1.1 " << status << " " << statusReason(status) << "\r\n";
	response << "Connection: close\r\n";
	if (status == 405)
		response << "Allow: GET\r\n";
	response << "Content-Length: " << length.str() << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
}

std::string EventLoop::buildErrorResponse(int status) const
{
	std::string body = statusReason(status);

	body += "\n";
	return buildResponse(status, body, "text/plain");
}

std::string EventLoop::buildFilePath(const LocationConfig& location,
		const std::string& requestPath) const
{
	std::string tail = requestPath;
	std::string root = location.root;

	if (requestPath.empty() || requestPath[0] != '/')
		return "";
	if (requestPath.find("..") != std::string::npos)
		return "";
	if (location.path != "/" && requestPath.compare(0, location.path.size(),
			location.path) == 0)
	{
		tail = requestPath.substr(location.path.size());
	}
	if (!tail.empty() && tail[0] == '/')
		tail.erase(0, 1);
	if (tail.empty() || tail[tail.size() - 1] == '/')
		tail += location.index.empty() ? "index.html" : location.index;
	if (!root.empty() && root[root.size() - 1] == '/')
		return root + tail;
	return root + "/" + tail;
}

bool EventLoop::readRegularFile(const std::string& path, std::string& body) const
{
	int fd = open(path.c_str(), O_RDONLY);
	char buffer[4096];

	if (fd < 0)
		return false;
	while (true)
	{
		ssize_t n = read(fd, buffer, sizeof(buffer));

		if (n < 0)
		{
			close(fd);
			return false;
		}
		if (n == 0)
			break;
		body.append(buffer, static_cast<std::size_t>(n));
	}
	close(fd);
	return true;
}

const char* EventLoop::statusReason(int status) const
{
	switch (status)
	{
	case 200:
		return "OK";
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
	case 500:
		return "Internal Server Error";
	case 501:
		return "Not Implemented";
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

	if (dot != std::string::npos)
		ext = path.substr(dot + 1);
	if (ext == "html" || ext == "htm")
		return "text/html";
	if (ext == "css")
		return "text/css";
	if (ext == "js")
		return "application/javascript";
	if (ext == "png")
		return "image/png";
	if (ext == "jpg" || ext == "jpeg")
		return "image/jpeg";
	if (ext == "gif")
		return "image/gif";
	return "text/plain";
}
