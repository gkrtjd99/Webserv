# Interface Agreement

이 문서는 3명이 `Webserv` 작업을 시작하기 전에 합의해야 하는 인터페이스와 공통 결정을 정리합니다. `docs/collaboration-guide.md` 의 Work Breakdown 절에서 말한 "공통 인터페이스 먼저 합의" 단계의 구체 항목입니다.

## Work Split

협업 가이드의 3-way 초기 분할을 그대로 채택하되 책임 경계를 분명히 합니다.

| Owner | Area | Primary Output |
| --- | --- | --- |
| A | HTTP parser, Request 모델, 라우팅 헬퍼 | `HttpRequest`, `HttpParser`, `matchServer`, `matchLocation` |
| B | Config parser, 설정 검증, 샘플 conf, 에러 페이지 정책 | `Config`, `ServerConfig`, `LocationConfig` |
| C | Socket/event loop, Connection 상태 머신, Response 직렬화, CGI/upload 통합 | `EventLoop`, `Connection`, `HttpResponse`, `CgiExecutor` |

통합 마일스톤은 두 단계로 나눕니다.

1. M1: 정적 GET 이 단일 server 에서 응답하는 시점. parser, config, event loop 가 처음 연결됩니다.
2. M2: POST, DELETE, upload, redirect, CGI, 에러 페이지가 모두 동작하는 시점.

## Pre-Split Decisions

아래 항목은 작업 분할 전에 한 번의 회의에서 결정합니다. 각 항목은 작은 PR 1~2 개 안에서 끝나야 합니다.

### Project Layout

- 디렉터리 구조 SHALL 합의합니다: `src/`, `include/`, `tests/`, `config/`, `www/`.
- 파일 명명 규칙 SHALL 통일합니다: 클래스 1개당 `ClassName.hpp` 와 `ClassName.cpp`.
- 헤더 가드 형식 SHALL 통일합니다: `WEBSERV_<MODULE>_<NAME>_HPP`.

### Build

- `Makefile` MUST 다음 규칙을 포함합니다: `$(NAME)`, `all`, `clean`, `fclean`, `re`.
- 컴파일러 플래그 MUST `-Wall -Wextra -Werror -std=c++98`.
- 의존 추적에 `-MMD -MP` 를 사용할지 SHALL 결정합니다.

### Coding Style

- 들여쓰기, 중괄호 위치, 식별자 표기법(snake_case vs camelCase) SHALL 합의합니다.
- C++98 금지 항목 MUST 명시합니다: `nullptr`, `auto`, range-for, `unique_ptr`, `unordered_map`, `to_string` 등.
- 외부 라이브러리 MUST NOT 사용합니다 (Boost 포함).

### Error Model

- Config 파싱 단계 SHALL `std::runtime_error` 계열 예외를 던집니다.
- 요청 처리 단계 MUST 예외를 던지지 않고 `HttpResponse` 의 status 로 표현합니다.
- `read`/`write` 이후 `errno` MUST NOT 참조합니다 (subject 요구사항).

### Common Types

- `enum HttpMethod { HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_UNKNOWN };` 를 공용 헤더에 둡니다.
- HTTP status 는 `int` 로 다루고, `const char* statusReason(int)` 헬퍼 한 곳에서만 텍스트를 결정합니다.
- 헤더 맵은 키를 lowercase 로 정규화한 `std::map<std::string, std::string>` SHALL 사용합니다.
- 바이트 버퍼는 binary 포함 모두 `std::string` 으로 통일합니다.

### File Descriptor Ownership

- RAII 래퍼 `Fd` 도입 여부 SHALL 결정합니다.
- `close()` 는 소유자만 호출합니다.
- CGI 파이프, listening socket, accepted socket 의 소유자 MUST 명시합니다.

### Logging

- `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` 매크로 또는 함수 한 가지 SHALL 사용합니다.
- 출력은 `std::cerr` 로 고정합니다.

## Public Interfaces

아래 시그니처는 합의 후 일주일 안에는 책임 경계를 바꾸지 않습니다. 인자 추가나 헬퍼 분리는 PR 안에서 자유롭게 가능합니다.

### HttpRequest

```cpp
class HttpRequest {
public:
    HttpMethod method() const;
    const std::string& methodString() const;
    const std::string& uri() const;
    const std::string& path() const;
    const std::string& query() const;
    const std::string& version() const;

    bool hasHeader(const std::string& name) const;
    const std::string& header(const std::string& name) const;
    const std::map<std::string, std::string>& headers() const;

    const std::string& body() const;
};
```

- 헤더 키는 lowercase 로 저장합니다.
- 바디는 `Content-Length` 와 `chunked` 를 모두 풀어 넣은 결과입니다 (CGI 는 unchunked 바디만 받습니다).

### HttpParser

```cpp
class HttpParser {
public:
    enum State { READING_HEAD, READING_BODY, COMPLETE, FAILED };

    void feed(const char* data, std::size_t len);
    void setBodyLimit(std::size_t n);
    State state() const;
    int errorStatus() const;
    const HttpRequest& request() const;
    void reset();
};
```

- `feed` 는 부분 입력을 누적하고, `COMPLETE` 또는 `FAILED` 가 될 때까지 반복 호출됩니다.
- `setBodyLimit` 은 routing 이후 확정된 body size limit을 Parser에 주입합니다.
- `errorStatus` 는 400, 413, 501, 505 같은 HTTP status 를 그대로 반환합니다.

### HttpResponse

```cpp
class HttpResponse {
public:
    void setStatus(int code);
    int status() const;
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);

    std::string serialize() const;
};
```

- `serialize` 가 `HTTP/1.1 200 OK\r\n...` 형식의 바이트열을 만듭니다.
- `Content-Length` 는 `setBody` 호출 시 자동 설정됩니다.

### Config

```cpp
struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    bool autoindex;
    std::set<HttpMethod> methods;
    std::pair<int, std::string> redirect;
    std::string uploadStore;
    std::map<std::string, std::string> cgi;
    std::size_t clientMaxBodySize;
};

struct ServerConfig {
    std::string host;
    int port;
    std::vector<std::string> serverNames;
    std::map<int, std::string> errorPages;
    std::size_t clientMaxBodySize;
    std::vector<LocationConfig> locations;
};

struct Config {
    std::vector<ServerConfig> servers;
    static Config parse(const std::string& path);
};
```

- `clientMaxBodySize` 가 location 단계에서 0 이면 server 단계로 fallback 합니다.
- 매칭 규칙은 별도 함수에서 구현합니다.

### Router

```cpp
const ServerConfig* matchServer(const std::vector<ServerConfig>& servers,
                                const std::string& host,
                                int port);

const LocationConfig* matchLocation(const ServerConfig& server,
                                    const std::string& path);
```

- 가장 긴 prefix 매칭을 사용합니다.
- 일치하는 항목이 없으면 `NULL` 을 반환합니다.
- `Host` 와 `serverNames` 비교는 lowercase 기준의 case-insensitive 비교를 사용합니다.

### EventLoop

```cpp
class EventLoop {
public:
    explicit EventLoop(const Config& config);
    void run();
};
```

- `run` 은 단일 `poll`/`kqueue`/`epoll`/`select` 루프를 돕니다.
- 종료는 `SIGINT` 또는 `SIGTERM` 시그널 핸들러가 플래그를 세트하면 됩니다.

### Connection

```cpp
class Connection {
public:
    enum State { READING, PROCESSING, WRITING, CLOSING };

    int fd() const;
    State state() const;
    HttpParser& parser();
    HttpResponse& response();
    std::string& readBuffer();
    std::string& writeBuffer();
    time_t lastActivity() const;
    void touch();
};
```

- 한 클라이언트당 하나의 인스턴스입니다.
- `EventLoop` 만 소유하고 직접 `close` 합니다.

### IHandler

```cpp
class IHandler {
public:
    virtual ~IHandler() {}
    virtual HttpResponse handle(const HttpRequest& req,
                                const LocationConfig& loc) = 0;
};
```

- 구현체: `StaticHandler`, `UploadHandler`, `RedirectHandler`, `CgiHandler`.

### CgiExecutor

```cpp
class CgiExecutor {
public:
    bool start(const HttpRequest& req, const LocationConfig& loc);
    int writeFd() const;
    int readFd() const;
    bool finished() const;
    HttpResponse buildResponse();
};
```

- `start` 는 `pipe`, `fork`, `execve` 를 수행하고 두 파이프를 non-blocking 으로 설정합니다.
- `EventLoop` 가 `writeFd` 와 `readFd` 를 동일한 `poll` 에 등록합니다.
- 입력 바디는 parser 단계에서 이미 unchunked 되어 있다고 가정합니다.

## Configuration File Schema

샘플 한 개를 함께 보고 합의합니다. `config/default.conf` 를 골든 파일로 두고 모든 팀이 이 파일을 참조해서 테스트합니다.

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 1m;
    error_page 404 /errors/404.html;

    location / {
        root ./www;
        index index.html;
        autoindex on;
        methods GET POST DELETE;
    }

    location /upload {
        root ./www;
        methods POST;
        upload_store ./uploads;
    }

    location /cgi-bin {
        root ./www/cgi-bin;
        methods GET POST;
        cgi .py /usr/bin/python3;
    }
}
```

- `client_max_body_size` 가 `1m`, `512k`, `1024` 같은 단위를 어디까지 지원할지 SHALL 결정합니다.
- 동일 host:port 쌍에 대한 중복 listen 은 에러로 처리합니다.

## Stubs and Mocks

각 팀이 다른 팀의 완성을 기다리지 않도록, 합의 직후 아래 stub PR 을 먼저 main 에 넣습니다.

- A: `HttpRequest` 와 `HttpParser` 의 빈 구현을 push 합니다. `feed` 는 항상 `COMPLETE` 을 만들고 고정 GET 을 반환하도록 두어도 됩니다.
- B: `Config::parse` 가 하드코딩된 1-server 설정을 반환하는 stub 을 push 합니다.
- C: `EventLoop::run` 이 단일 accept 후 고정 응답을 보내는 minimal 구현을 push 합니다.

위 3 개 stub 이 main 에 들어가면 각자 병렬로 진행합니다.

## Integration Order

1. A 의 parser 가 `HttpRequest` 를 채우면 C 의 event loop 가 그것을 받아 handler 로 넘깁니다.
2. B 의 `Config` 가 들어오면 C 의 `EventLoop` 생성자가 그것을 받습니다.
3. Static GET 이 끝나면 POST, DELETE, upload, redirect 순서로 추가합니다.
4. CGI 는 마지막에 통합합니다. 단일 poll 에 파이프 fd 두 개를 추가로 등록하는 부분이 가장 까다롭습니다.

## Open Questions

회의 전에 각자 한 번씩 답을 가져옵니다.

- HTTP/1.1 의 어디까지 지원할지 (keep-alive, pipelining 포함 여부).
- chunked 응답을 보낼지, 항상 `Content-Length` 로 응답할지.
- Directory listing HTML 의 형식.
- Default error page 의 위치와 형식.
- Idle connection timeout 기본값 (예: 60초).
- 최대 동시 연결 수 기본값.
- macOS 에서 `fcntl` 호출을 어떤 함수로 감쌀지 (subject 의 `F_SETFL`, `O_NONBLOCK`, `FD_CLOEXEC` 제한 준수).

## References

- [docs/collaboration-guide.md](collaboration-guide.md)
- [en.subject.pdf](../en.subject.pdf)
- [HTTP/1.1 RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230)
- [NGINX Configuration Reference](https://nginx.org/en/docs/dirindex.html)
