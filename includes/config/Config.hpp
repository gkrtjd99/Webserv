#ifndef WEBSERV_CONFIG_CONFIG_HPP
#define WEBSERV_CONFIG_CONFIG_HPP

#include <string>
#include <vector>

#include "ServerConfig.hpp"

// Config::parse(path) 가 정상 반환했을 때 다른 모듈(A 의 router, C 의
// EventLoop) 이 가정해도 되는 post-parse invariants:
//
//   - servers 가 비어있지 않다 (V-G-1).
//   - 모든 ServerConfig.host 는 비어있지 않고 형식이 검증되었다
//     (IPv4, "localhost", 또는 hostname 패턴).
//   - 모든 ServerConfig.port 는 1..65535.
//   - ServerConfig.clientMaxBodySize 는 사용자 값 또는 1m 디폴트가
//     채워져 있다. clientMaxBodySizeSet 으로 explicit 0 과 미설정을
//     구분할 수 있다.
//   - errorPages 의 키는 300..599, 값은 '/' 로 시작.
//   - 같은 (host, port) 그룹의 첫 번째 server 가 default. 이후 server
//     는 server_name 으로 구분된다 (V-G-3).
//   - 각 server 의 locations 에는 동일 path 가 두 번 이상 등장하지
//     않는다 (V-L-3 dedupe 적용 후 마지막만 유지).
//   - LocationConfig.path 는 '/' 로 시작하고 끝의 '/' 는 제거됨
//     (단, '/' 자체는 유지). matchLocation 입력으로 그대로 사용 가능.
//   - LocationConfig.root 가 비어있지 않으면 디스크에 존재하고 read
//     권한이 있다 (단, redirect 가 설정된 location 은 root 가 비어도
//     OK).
//   - LocationConfig.methods 는 비어있지 않다 ({GET} 디폴트).
//   - LocationConfig.index 는 비어있지 않다 ("index.html" 디폴트).
//   - LocationConfig.clientMaxBodySize 는 사용자 값 또는 server 값이
//     fallback 되어 있다 (그대로 사용 가능, 추가 fallback 불필요).
//   - cgi 의 모든 인터프리터는 실행 권한이 있는 절대 경로.
//   - uploadStore 가 비어있지 않으면 디스크에 존재하고 write 권한이
//     있으며, 해당 location 의 methods 에 POST 가 포함된다.
struct Config {
	std::vector<ServerConfig> servers;

	static Config parse(const std::string& path);
};

#endif
