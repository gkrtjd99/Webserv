#ifndef REQUESTDISPATCHER_HPP
# define REQUESTDISPATCHER_HPP

#include "AutoindexBuilder.hpp"
#include "CgiTypes.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "DeleteHandler.hpp"
#include "FileResource.hpp"
#include "HttpMethod.hpp"
#include "HttpRequest.hpp"
#include "PathResolver.hpp"
#include "ResponseBuilder.hpp"
#include "StaticHandler.hpp"
#include "UploadHandler.hpp"

#include <map>
#include <string>
#include <vector>

class RequestDispatcher
{
public:
	enum Action
	{
		WRITE_RESPONSE,
		START_CGI
	};

	struct Result
	{
		Action action;
		std::string response;
		const ServerConfig* server;
		const LocationConfig* location;
		CgiMatch cgiMatch;
		bool closeAfterWrite;

		Result();
	};

	explicit RequestDispatcher(const std::vector<ServerConfig>& servers);

	Result dispatch(Connection& connection, const HttpRequest& request);
	void prepareBodyLimit(Connection& connection);
	std::string buildParserError(Connection& connection, int status) const;
	std::string buildCgiError(Connection& connection, int status) const;

private:
	struct RequestContext
	{
		Connection* connection;
		const HttpRequest* request;
		const ServerConfig* server;
		const LocationConfig* location;
		bool keepAlive;
	};

	typedef std::string (RequestDispatcher::*MethodHandler)(
			const RequestContext&);

	const std::vector<ServerConfig>& _servers;
	PathResolver _paths;
	FileResource _files;
	AutoindexBuilder _autoindex;
	ResponseBuilder _responses;
	StaticHandler _staticHandler;
	UploadHandler _uploadHandler;
	DeleteHandler _deleteHandler;
	std::map<HttpMethod, MethodHandler> _methodHandlers;

	RequestDispatcher(const RequestDispatcher& other);
	RequestDispatcher& operator=(const RequestDispatcher& other);

	void registerMethodHandlers();
	std::string handleGet(const RequestContext& context);
	std::string handlePost(const RequestContext& context);
	std::string handleDelete(const RequestContext& context);
	bool locationAllowsMethod(const LocationConfig& location,
			HttpMethod method) const;
	bool shouldKeepAlive(const HttpRequest& request) const;
	const ServerConfig* resolveServer(Connection& connection) const;
	const LocationConfig* resolveLocation(Connection& connection) const;
	const ServerConfig* defaultServerForPort(int port) const;
};

#endif
