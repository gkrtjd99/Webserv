#include "RequestDispatcher.hpp"
#include "HttpHelper.hpp"
#include "Router.hpp"

RequestDispatcher::Result::Result()
	: action(WRITE_RESPONSE),
	  response(),
	  server(NULL),
	  location(NULL),
	  cgiMatch(),
	  closeAfterWrite(true)
{
}

RequestDispatcher::RequestDispatcher(
		const std::vector<ServerConfig>& servers)
	: _servers(servers),
	  _paths(),
	  _files(),
	  _autoindex(),
	  _responses(),
	  _staticHandler(_paths, _files, _autoindex, _responses),
	  _uploadHandler(_paths, _responses),
	  _deleteHandler(_paths, _files, _responses),
	  _methodHandlers()
{
	registerMethodHandlers();
}

RequestDispatcher::Result RequestDispatcher::dispatch(
		Connection& connection,
		const HttpRequest& request)
{
	Result result;
	RequestContext context;
	MethodHandler handler;
	std::map<HttpMethod, MethodHandler>::const_iterator it;

	result.server = resolveServer(connection);
	result.location = resolveLocation(connection);
	result.closeAfterWrite = !shouldKeepAlive(request);
	if(result.server == NULL)
	{
		result.response = _responses.buildError(500, NULL, false);
		result.closeAfterWrite = true;
		return result;
	}
	if(result.location == NULL)
	{
		result.response = _responses.buildError(404, result.server,
				!result.closeAfterWrite);
		return result;
	}
	if(result.location->redirect.first != 0)
	{
		result.response = _responses.buildRedirect(*result.location,
				!result.closeAfterWrite);
		return result;
	}
	if(!locationAllowsMethod(*result.location, request.method()))
	{
		result.response = _responses.buildMethodNotAllowed(*result.location,
				!result.closeAfterWrite);
		return result;
	}
	if(_paths.findCgiScript(request, *result.location, result.cgiMatch))
	{
		result.action = START_CGI;
		return result;
	}
	it = _methodHandlers.find(request.method());
	if(it == _methodHandlers.end())
	{
		result.response = _responses.buildError(501, result.server,
				!result.closeAfterWrite);
		return result;
	}
	context.connection = &connection;
	context.request = &request;
	context.server = result.server;
	context.location = result.location;
	context.keepAlive = !result.closeAfterWrite;
	handler = it->second;
	result.response = (this->*handler)(context);
	return result;
}

void RequestDispatcher::prepareBodyLimit(Connection& connection)
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

std::string RequestDispatcher::buildParserError(Connection& connection,
		int status) const
{
	const ServerConfig* server = connection.server();

	if(server == NULL)
	{
		server = defaultServerForPort(connection.localPort());
	}
	return _responses.buildError(status, server, false);
}

std::string RequestDispatcher::buildCgiError(Connection& connection,
		int status) const
{
	return _responses.buildError(status, connection.server(),
			!connection.closeAfterWrite());
}

void RequestDispatcher::registerMethodHandlers()
{
	_methodHandlers[HTTP_GET] = &RequestDispatcher::handleGet;
	_methodHandlers[HTTP_POST] = &RequestDispatcher::handlePost;
	_methodHandlers[HTTP_DELETE] = &RequestDispatcher::handleDelete;
}

std::string RequestDispatcher::handleGet(const RequestContext& context)
{
	(void)context.connection;
	return _staticHandler.handle(*context.request, *context.server,
			*context.location, context.keepAlive);
}

std::string RequestDispatcher::handlePost(const RequestContext& context)
{
	(void)context.connection;
	return _uploadHandler.handle(*context.request, *context.server,
			*context.location, context.keepAlive);
}

std::string RequestDispatcher::handleDelete(const RequestContext& context)
{
	(void)context.connection;
	return _deleteHandler.handle(*context.request, *context.server,
			*context.location, context.keepAlive);
}

bool RequestDispatcher::locationAllowsMethod(const LocationConfig& location,
		HttpMethod method) const
{
	return location.methods.find(method) != location.methods.end();
}

bool RequestDispatcher::shouldKeepAlive(const HttpRequest& request) const
{
	std::string connection = HttpHelper::toLowerString(
			HttpHelper::trim(request.header("connection")));

	return request.version() == "HTTP/1.1" && connection != "close";
}

const ServerConfig* RequestDispatcher::resolveServer(
		Connection& connection) const
{
	const HttpRequest& request = connection.parser().request();
	const ServerConfig* server;

	if(connection.server() != NULL && request.getHost().empty())
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

const LocationConfig* RequestDispatcher::resolveLocation(
		Connection& connection) const
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

const ServerConfig* RequestDispatcher::defaultServerForPort(int port) const
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
