#ifndef CONFIG_HPP
# define CONFIG_HPP

#include <string>
#include <vector>

/**
 * B 확인 필요: 최종 LocationConfig 는 root, index 외에 methods,
 * autoindex, redirect, upload_store, cgi, body limit 값을 채워야 한다.
 */
struct LocationConfig
{
	std::string path;
	std::string root;
	std::string index;
};

/**
 * B 확인 필요: 최종 ServerConfig 는 host, error_page,
 * client_max_body_size 기본값과 여러 server block 검증을 포함해야 한다.
 */
struct ServerConfig
{
	int port;
	std::size_t clientMaxBodySize;
	std::vector<std::string> serverNames;
	std::vector<LocationConfig> locations;
};

struct Config
{
	std::vector<ServerConfig> servers;

	/**
	 * config 파일을 읽어서 현재 구현에 필요한 ServerConfig 목록을 만든다.
	 */
	static Config parse(const std::string& path);
};

#endif
