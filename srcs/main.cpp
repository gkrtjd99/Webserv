#include "Config.hpp"
#include "EventLoop.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

/**
 * config 파일 경로는 argv[1] 만 사용한다.
 * B 확인 필요: subject 의 default path 허용 정책을 쓸지, 인자 없으면 실패할지 최종 결정한다.
 */
int main(int argc, char** argv)
{
	Config config;

	if (argc != 2)
	{
		std::cerr << "usage: ./webserv [configuration file]" << std::endl;
		return EXIT_FAILURE;
	}
	try
	{
		const std::string configPath = argv[1];

		if (configPath.empty())
			throw std::runtime_error("empty configuration file path");
		config = Config::parse(configPath);
		if (config.servers.empty())
			throw std::runtime_error("no server configuration");
		EventLoop eventLoop(config);

		eventLoop.run();
	}
	catch (const std::exception& error)
	{
		std::cerr << "webserv: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
