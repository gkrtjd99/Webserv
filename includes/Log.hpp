#ifndef WEBSERV_LOG_HPP
#define WEBSERV_LOG_HPP

#include <iostream>

// stderr 로 한 줄짜리 진단을 남긴다. interface-agreement 의 LOG_*  슬롯.
// 함수 호출이 아닌 매크로라서 args 안에 << 연산을 자유롭게 쓸 수 있다.
#define LOG_INFO(args)  do { std::cerr << "[INFO] "  << args << "\n"; } while (0)
#define LOG_WARN(args)  do { std::cerr << "[WARN] "  << args << "\n"; } while (0)
#define LOG_ERROR(args) do { std::cerr << "[ERROR] " << args << "\n"; } while (0)

#endif
