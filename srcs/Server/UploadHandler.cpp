#include "UploadHandler.hpp"

#include <cstdio>
#include <ctime>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>

UploadHandler::UploadHandler(const PathResolver& paths,
		const ResponseBuilder& responses)
	: _paths(paths),
	  _responses(responses),
	  _sequence(0)
{
}

std::string UploadHandler::handle(const HttpRequest& request,
		const ServerConfig& server,
		const LocationConfig& location,
		bool keepAlive)
{
	if(location.uploadStore.empty())
	{
		return _responses.buildError(501, &server, keepAlive);
	}
	for(int attempt = 0; attempt < 1000; attempt++)
	{
		std::string filename = "upload-"
			+ toString(static_cast<std::size_t>(std::time(NULL))) + "-"
			+ toString(_sequence++) + ".bin";
		std::string path = _paths.joinPath(location.uploadStore, filename);
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
				return _responses.buildError(500, &server, keepAlive);
			}
			offset += static_cast<std::size_t>(n);
		}
		close(fd);
		std::string body = "Created\n";
		std::string uri = location.path == "/"
			? "/" + filename : location.path + "/" + filename;
		std::string headers = "Location: " + uri + "\r\n";

		return _responses.buildWithHeaders(201, body, "text/plain", headers,
				keepAlive);
	}
	return _responses.buildError(500, &server, keepAlive);
}

std::string UploadHandler::toString(std::size_t value) const
{
	std::ostringstream oss;

	oss << value;
	return oss.str();
}
