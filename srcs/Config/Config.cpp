#include "Config.hpp"

#include <cctype>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

static bool isNameChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static bool isSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r'
		|| c == '\v' || c == '\f';
}

static std::string readConfigFile(const std::string& path)
{
	int fd = open(path.c_str(), O_RDONLY);
	char buffer[4096];
	std::string content;

	if (fd < 0)
		throw std::runtime_error("configuration file open failed");
	while (true)
	{
		ssize_t n = read(fd, buffer, sizeof(buffer));

		if (n < 0)
		{
			close(fd);
			throw std::runtime_error("configuration file read failed");
		}
		if (n == 0)
			break;
		content.append(buffer, static_cast<std::size_t>(n));
	}
	close(fd);
	return content;
}

static int parseListenPort(const std::string& content)
{
	std::size_t pos = content.find("listen");

	while (pos != std::string::npos)
	{
		const bool leftOk = pos == 0 || !isNameChar(content[pos - 1]);
		const std::size_t end = pos + 6;
		const bool rightOk = end >= content.size() || !isNameChar(content[end]);

		if (leftOk && rightOk)
			break;
		pos = content.find("listen", pos + 1);
	}
	if (pos == std::string::npos)
		throw std::runtime_error("listen directive is missing");
	pos += 6;
	while (pos < content.size() && isSpace(content[pos]))
		++pos;

	std::size_t semicolon = content.find(';', pos);
	if (semicolon == std::string::npos)
		throw std::runtime_error("listen directive is not terminated");

	std::string value = content.substr(pos, semicolon - pos);
	std::size_t colon = value.rfind(':');
	if (colon != std::string::npos)
		value = value.substr(colon + 1);

	std::size_t begin = 0;
	while (begin < value.size() && isSpace(value[begin]))
		++begin;

	std::size_t end = value.size();
	while (end > begin && isSpace(value[end - 1]))
		--end;
	if (begin == end)
		throw std::runtime_error("listen port is missing");

	int port = 0;
	for (std::size_t i = begin; i < end; ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			throw std::runtime_error("listen port is invalid");
		port = port * 10 + (value[i] - '0');
		if (port > 65535)
			throw std::runtime_error("listen port is out of range");
	}
	if (port == 0)
		throw std::runtime_error("listen port is out of range");
	return port;
}

static std::string parseStringDirective(const std::string& content,
		const std::string& name,
		const std::string& defaultValue)
{
	std::size_t pos = content.find(name);

	while (pos != std::string::npos)
	{
		const bool leftOk = pos == 0 || !isNameChar(content[pos - 1]);
		const std::size_t nameEnd = pos + name.size();
		const bool rightOk = nameEnd >= content.size()
			|| !isNameChar(content[nameEnd]);

		if (leftOk && rightOk)
			break;
		pos = content.find(name, pos + 1);
	}
	if (pos == std::string::npos)
		return defaultValue;
	pos += name.size();
	while (pos < content.size() && isSpace(content[pos]))
		++pos;

	std::size_t semicolon = content.find(';', pos);
	if (semicolon == std::string::npos)
		throw std::runtime_error(name + " directive is not terminated");

	std::size_t begin = pos;
	while (begin < semicolon && isSpace(content[begin]))
		++begin;

	std::size_t end = semicolon;
	while (end > begin && isSpace(content[end - 1]))
		--end;
	if (begin == end)
		return defaultValue;
	return content.substr(begin, end - begin);
}

Config Config::parse(const std::string& path)
{
	Config config;
	ServerConfig server;
	LocationConfig location;
	std::string content = readConfigFile(path);

	/**
	 * B 확인 필요: 현재는 C 통합용 최소 parser 이다.
	 * 최종 Config parser 에서는 server/location block, methods, error_page,
	 * upload_store, cgi, client_max_body_size 를 채워줘야 한다.
	 */
	server.port = parseListenPort(content);
	server.clientMaxBodySize = 1048576;
	server.serverNames.push_back("localhost");
	location.path = "/";
	location.root = parseStringDirective(content, "root", "./www");
	location.index = parseStringDirective(content, "index", "index.html");
	server.locations.push_back(location);
	config.servers.push_back(server);
	return config;
}
