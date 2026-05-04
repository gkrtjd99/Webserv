#ifndef EVENTLOOP_HPP
# define EVENTLOOP_HPP

#include "Config.hpp"
#include "Connection.hpp"

#include <map>
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

	void run();

private:
	std::vector<ServerConfig> _servers;
	std::map<int, int> _listenPorts;
	std::map<int, Connection> _connections;
	std::map<int, int> _cgiInputToClient;
	std::map<int, int> _cgiOutputToClient;
	std::size_t _uploadSequence;

	EventLoop(const EventLoop& other);
	EventLoop& operator=(const EventLoop& other);

	void openListenSockets();
	int openListenSocket(const std::string& host, int port);
	void setNonBlocking(int fd);
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
	const ServerConfig* resolveServer(Connection& connection);
	const LocationConfig* resolveLocation(Connection& connection);
	const ServerConfig* defaultServerForPort(int port) const;
	void startCgi(int fd,
			Connection& connection,
			const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			const CgiMatch& match);
	void failCgi(int fd, int status);
	void registerCgiFds(int clientFd, const CgiExecutor& cgi);
	void unregisterCgiFds(int clientFd);
	bool hasActiveCgi() const;
	void checkCgiTimeouts();
	std::string handleRequest(Connection& connection,
			const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location);
	std::string handleGet(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive) const;
	std::string handleUpload(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive);
	std::string handleDelete(const HttpRequest& request,
			const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive) const;
	std::string handleRedirect(const LocationConfig& location,
			bool keepAlive) const;
	std::string buildAutoindex(const HttpRequest& request,
			const LocationConfig& location,
			const std::string& directoryPath,
			bool keepAlive) const;
	std::string build405Response(const ServerConfig& server,
			const LocationConfig& location,
			bool keepAlive) const;
	std::string buildResponse(int status,
			const std::string& body,
			const std::string& contentType,
			bool keepAlive) const;
	std::string buildResponseWithHeaders(int status,
			const std::string& body,
			const std::string& contentType,
			const std::string& extraHeaders,
			bool keepAlive) const;
	std::string buildErrorResponse(int status,
			const ServerConfig* server,
			bool keepAlive) const;
	bool locationAllowsMethod(const LocationConfig& location,
			HttpMethod method) const;
	bool shouldKeepAlive(const HttpRequest& request) const;
	bool findCgiScript(const HttpRequest& request,
			const LocationConfig& location,
			CgiMatch& match) const;
	std::string buildFilePath(const LocationConfig& location,
			const std::string& requestPath) const;
	std::string buildFilePathFromUri(const ServerConfig& server,
			const std::string& uriPath) const;
	bool resolvePathInsideRoot(const LocationConfig& location,
			const std::string& rawPath,
			std::string& resolvedPath) const;
	bool readRegularFile(const std::string& path, std::string& body) const;
	bool removeRegularFile(const std::string& path) const;
	const char* statusReason(int status) const;
	const char* contentTypeForPath(const std::string& path) const;
};

#endif
