# Subject Requirements and Extension Plan

이 문서는 `en.subject.pdf` version 24.0을 기준으로 mandatory 요구사항, 허용 함수, bonus 확장 설계를 정리한다. 구현 판단은 이 문서를 우선 기준으로 삼고, 세부 HTTP 동작은 RFC/NGINX 비교와 팀 합의 문서로 보완한다.

## 1. 제출/빌드 계약

제출 대상:

- `Makefile`
- `*.h`, `*.hpp`, `*.cpp`, `*.tpp`, `*.ipp`
- configuration files

Makefile 필수 rule:

- `$(NAME)`
- `all`
- `clean`
- `fclean`
- `re`

빌드 조건:

- executable name: `webserv`
- compiler: `c++`
- flags: `-Wall -Wextra -Werror`
- C++98 준수
- external library 금지
- Boost 금지
- 불필요한 relink 금지

실행 형태:

```text
./webserv [configuration file]
```

config path는 command line 인자로 받거나 default path를 사용할 수 있다. 현재 프로젝트 정책은 인자 하나를 요구하는 쪽이다.

## 2. 허용 함수

subject PDF의 external function 목록:

```text
execve, pipe, strerror, gai_strerror, errno, dup, dup2, fork, socketpair
htons, htonl, ntohs, ntohl
select, poll
epoll_create, epoll_ctl, epoll_wait
kqueue, kevent
socket, accept, listen, send, recv, chdir, bind, connect
getaddrinfo, freeaddrinfo, setsockopt, getsockname, getprotobyname
fcntl, close, read, write, waitpid, kill, signal
access, stat, open, opendir, readdir, closedir
```

프로젝트 정책:

- I/O multiplexing은 `poll()` 하나로 통일한다.
- `select`, `epoll`, `kqueue`는 subject상 허용되지만 구현을 플랫폼별로 나누지 않기 위해 쓰지 않는다.
- Linux 전용 convenience API는 쓰지 않는다.

허용 목록에 없으므로 사용하지 않을 함수:

```text
realpath, lstat, readlink, openat
unlink, remove, rename
inet_ntoa, inet_ntop
sendfile, accept4, pipe2
```

현재 로컬 코드에서 subject strict mode로 교체해야 하는 사용:

| 위치 | 현재 사용 | 교체 방향 |
| --- | --- | --- |
| `EventLoop` path 검증 | `realpath` | lexical normalized relative path 검증 |
| `EventLoop` client address | `inet_ntoa` | `ntohl` + C++ 문자열 변환 |
| `ConfigParser` include canonicalization | `realpath` | lexical include stack 또는 absolute-path 문자열 기준 |

`std::time` 같은 C++98 표준 라이브러리 기능은 external function 목록과 같은 층위로 금지하지 않는다. timeout 구현은 `std::time` 또는 `poll()` tick 방식 중 이식성과 테스트 편의성을 보고 선택한다.

## 3. Non-Blocking I/O 규칙

mandatory 핵심 규칙:

- server는 항상 non-blocking이어야 한다.
- client/server 사이의 모든 I/O는 하나의 `poll()` 또는 equivalent로 관리한다.
- listen fd도 같은 `poll()` 대상이다.
- `poll()`은 read와 write를 동시에 모니터링해야 한다.
- `poll()` readiness 없이 socket/pipe/FIFO에 `read`, `recv`, `write`, `send`를 호출하면 안 된다.
- regular disk file은 예외다. file `read()`/`write()`는 `poll()` readiness가 필요 없다.
- request가 무기한 hang되면 안 된다.
- client disconnect를 적절히 처리해야 한다.

`errno` 정책:

- subject는 read/write 이후 `errno` 값을 보고 server behavior를 조정하는 것을 금지한다.
- 따라서 socket/pipe I/O wrapper는 `n > 0`, `n == 0`, `n < 0`만 분류한다.
- `EAGAIN`, `EWOULDBLOCK`, `EINTR` 분기는 하지 않는다.
- `n < 0`은 fatal I/O error로 처리한다.

권장 wrapper:

```cpp
enum IoStatus
{
    IO_OK,
    IO_EOF,
    IO_ERROR
};
```

## 4. Mandatory 기능 요구사항

서버 기본 요구사항:

- config file을 사용한다.
- 다른 web server를 `execve`하면 안 된다.
- standard browser와 호환되어야 한다.
- NGINX와 header/response behavior를 비교할 수 있어야 한다.
- HTTP response status code가 정확해야 한다.
- custom error page가 없으면 default error page를 제공해야 한다.
- `fork()`는 CGI 실행에만 사용한다.
- fully static website를 serve할 수 있어야 한다.
- client file upload를 지원해야 한다.
- 최소 `GET`, `POST`, `DELETE` method를 지원해야 한다.
- stress test에서 server가 계속 available해야 한다.
- multiple port를 listen해서 다른 content를 제공할 수 있어야 한다.

HTTP 범위:

- 전체 RFC 구현은 요구하지 않는다.
- HTTP/1.0을 참고점으로 제시하지만 강제는 아니다.
- 현재 프로젝트는 HTTP/1.1 parser와 `Host` 기반 routing helper를 가진다.
- virtual host는 subject에서 out of scope라고 명시하지만 구현 가능하다. 이미 A/B 계약에는 `matchServer`와 `server_name`이 있으므로 확장 대상으로 둔다.

## 5. Configuration File 요구사항

NGINX의 `server` block에서 영감을 받을 수 있다.

config에서 표현해야 하는 것:

- server가 listen할 모든 interface:port pair
- default error pages
- client request body 최대 크기
- URL/route별 설정. regex는 필요 없다.

route별 설정:

- accepted HTTP methods
- HTTP redirection
- requested file을 찾을 root directory
- directory listing on/off
- directory 요청 시 serve할 default file
- upload 허용 여부와 저장 위치
- file extension 기반 CGI execution

CGI 관련 세부 요구사항:

- CGI 환경 변수를 주의해서 구성한다.
- client가 제공한 full request와 arguments가 CGI에서 사용 가능해야 한다.
- chunked request는 server가 unchunk한 뒤 CGI stdin에 전달한다.
- CGI는 EOF로 request body 종료를 인식해야 한다.
- CGI output에 `Content-Length`가 없으면 EOF가 response data 종료를 의미한다.
- CGI는 relative path file access가 맞도록 correct directory에서 실행되어야 한다.
- 최소 하나의 CGI, 예를 들어 Python 또는 php-CGI를 지원해야 한다.

평가 시 필요한 파일:

- 모든 기능을 테스트/시연할 configuration files
- default files
- static site files
- upload demonstration files
- CGI demonstration files

## 6. README 요구사항

repository root에 `README.md`가 있어야 한다.

README 필수 조건:

- 첫 줄은 italic으로 시작해야 한다.
- 첫 줄 내용은 `This project has been created as part of the 42 curriculum by <login...>.`
- `Description` section
- `Instructions` section
- `Resources` section
- AI 사용 내역 설명
- 영어로 작성

권장 추가 section:

- feature list
- configuration examples
- test commands
- architecture overview
- known limitations
- subject compliance notes

## 7. Mandatory 구현 항목별 설계

| 항목 | 구현 방향 | 확장 포인트 |
| --- | --- | --- |
| static website | `GET` static handler | content type table 확장 |
| upload | `POST` upload handler | filename policy, storage backend 분리 |
| DELETE | 삭제 syscall 정책 결정 필요 | subject strict/pragmatic mode 분리 |
| error pages | `ErrorResponder` | status별 default/custom source 교체 |
| multiple ports | listen endpoint table | multiple server group으로 확장 |
| route methods | method allow-list | 새 method enum/handler 등록 |
| redirect | pre-handler route action | return code/target 정책 확장 |
| autoindex | directory handler | HTML renderer 교체 |
| CGI | extension-based dispatcher | multiple CGI types와 bonus로 확장 |
| timeout | `poll()` tick 기반 | client/CGI idle policy 분리 |
| browser compatibility | HTTP response serializer | keep-alive/header 정책 확장 |

DELETE 주의:

- subject는 `DELETE` method 지원을 mandatory로 요구한다.
- 하지만 허용 함수 목록에 `unlink`, `remove`, `rename`이 없다.
- strict subject mode에서는 실제 파일 삭제를 허용 함수만으로 구현할 수 없다.
- 팀은 평가 전에 다음 중 하나를 결정해야 한다.
  - strict mode: DELETE의 한계를 문서화하고 평가자에게 함수 목록 충돌을 설명한다.
  - pragmatic mode: `unlink()` 사용을 팀/평가자와 합의하고 실제 삭제를 구현한다.

## 8. Extensible Runtime Architecture

새 HTTP method나 bonus 기능이 추가될 때 `EventLoop` 조건문이 계속 늘어나지 않는 구조를 목표로 한다.

공통 request context:

```cpp
struct RequestContext
{
    const ServerConfig* server;
    const LocationConfig* location;
    const HttpRequest* request;
    std::string remoteAddress;
};
```

공통 처리 순서:

```text
match listen endpoint
match server
match location
apply redirect
check method allowed
match CGI
dispatch method handler
fallback 501
```

method handler registry:

```cpp
typedef std::string (EventLoop::*MethodHandler)(const RequestContext&) const;

std::map<HttpMethod, MethodHandler> _methodHandlers;
```

초기 mandatory 등록:

```cpp
_methodHandlers[HTTP_GET] = &EventLoop::handleStaticGet;
_methodHandlers[HTTP_POST] = &EventLoop::handleUploadPost;
_methodHandlers[HTTP_DELETE] = &EventLoop::handleDelete;
```

새 method 추가 절차:

1. A와 `HttpMethod` enum/parser 지원 합의
2. B와 config `methods` 문법 지원 합의
3. C에 handler 함수 추가
4. registry에 한 줄 등록
5. status mapping과 tests 추가

## 9. Bonus 요구사항

subject bonus:

- cookies and session management 지원. simple examples 제공
- multiple CGI types 처리

bonus는 mandatory가 완전히 문제 없이 통과해야 평가된다. 따라서 bonus 구현은 mandatory path를 흔들지 않는 plugin-style 확장으로 둔다.

## 10. Bonus 확장 설계

### Cookies

cookie parsing은 HTTP parser의 core syntax와 분리한다.

추가 구조:

```cpp
class CookieJar
{
public:
    void parse(const std::string& cookieHeader);
    bool has(const std::string& name) const;
    const std::string& get(const std::string& name) const;
};
```

response cookie 생성은 response serializer의 optional header로 처리한다.

```cpp
class ResponseHeaders
{
public:
    void addSetCookie(const std::string& serializedCookie);
};
```

확장 원칙:

- mandatory handler는 cookie를 몰라도 동작한다.
- session이 필요한 handler만 `RequestContext`에서 cookie/session service를 읽는다.
- cookie parser 실패는 request 전체 실패로 바로 보지 않고, session 기능에서만 invalid cookie로 처리한다.

### Session Management

session은 storage interface 뒤에 숨긴다.

```cpp
class SessionStore
{
public:
    std::string create();
    bool exists(const std::string& id) const;
    void set(const std::string& id,
             const std::string& key,
             const std::string& value);
    std::string get(const std::string& id,
                    const std::string& key) const;
};
```

초기 구현은 in-memory map으로 충분하다.

확장 포인트:

- memory store
- file-backed store
- session expiration
- route별 session required option

subject 허용 함수 관점:

- file-backed session은 `open/read/write/close/stat/access` 범위에서만 구현한다.
- expiration은 `std::time`, `poll()` tick, request counter 중 프로젝트 정책에 맞는 방식으로 처리한다.

### Multiple CGI Types

현재 mandatory CGI도 extension -> interpreter map 구조다. bonus는 이 구조를 그대로 확장한다.

config 예:

```nginx
location /cgi-bin {
    root ./www/cgi-bin;
    methods GET POST;
    cgi .py /usr/bin/python3;
    cgi .php /usr/bin/php-cgi;
}
```

설계:

```cpp
struct CgiRuntime
{
    std::string extension;
    std::string interpreter;
    std::vector<std::string> fixedArgs;
    std::map<std::string, std::string> extraEnv;
};
```

확장 원칙:

- CGI 판별은 extension matcher 하나가 담당한다.
- interpreter별 env 차이는 `CgiRuntime` 설정으로 흡수한다.
- `CgiExecutor`는 interpreter 종류를 몰라야 한다.
- process 실행 흐름은 mandatory CGI와 동일하게 `pipe/fork/dup2/execve/poll`을 사용한다.

## 11. 구현 순서

1. Subject compliance 정리
   - `realpath`, `inet_ntoa` 제거 또는 허용 여부 확인
   - read/write 후 `errno` 분기 금지 정책 확정
2. Runtime foundation
   - `RequestContext`
   - method handler registry
   - shared response/error responder
3. Multiple listen endpoints
   - interface:port pair table
   - listen fd map
   - server candidate group
4. Mandatory route features
   - custom/default error pages
   - upload
   - DELETE policy
   - autoindex
5. Keep-alive/pipelining
   - parser buffer preservation
   - response 후 connection reuse
6. CGI hardening
   - correct working directory policy
   - EOF-based CGI output handling
   - multiple CGI type-ready matcher
7. README and demonstration assets
   - root README
   - sample configs
   - static/upload/CGI demo files
8. Bonus
   - cookies
   - sessions
   - multiple CGI types
