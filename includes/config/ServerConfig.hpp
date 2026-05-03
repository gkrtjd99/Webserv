#ifndef WEBSERV_CONFIG_SERVER_CONFIG_HPP
#define WEBSERV_CONFIG_SERVER_CONFIG_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "LocationConfig.hpp"

struct ServerConfig {
	std::string                 host;
	int                         port;
	std::vector<std::string>    serverNames;
	std::map<int, std::string>  errorPages;
	std::size_t                 clientMaxBodySize;
	bool                        clientMaxBodySizeSet;    // V-S-5: explicit 0 과 미설정 구분
	std::vector<LocationConfig> locations;

	ServerConfig();

	// 두 필드를 항상 함께 갱신하기 위한 헬퍼.
	// 직접 ServerConfig 를 만들어 검증기를 돌릴 때(예: 단위 테스트)
	// clientMaxBodySize 만 할당하면 미설정으로 취급되어 디폴트로 덮인다.
	void setClientMaxBodySize(std::size_t value);
};

#endif
