# CGI Implementation Plan

이 문서는 `server` 브랜치에서 CGI 필수 구현, CGI 환경 변수 구성, CGI 라우팅/판별을 마무리하기 위한 작업 계획이다.

현재 `main` 기준 서버 런타임은 static GET skeleton이다. client fd만 `poll()`에 등록하고, `Connection` 상태도 `READING`/`WRITING`만 가진다. CGI 구현은 이 구조를 유지하되, CGI pipe fd와 child process를 같은 `poll()` 루프에 추가하는 방향으로 진행한다.

## 1. 목표 범위

이번 CGI 작업에서 반드시 끝낼 범위는 아래다.

- `CgiExecutor` 추가
- `pipe`, `fork`, `dup2`, `execve` 기반 CGI 실행
- CGI stdin/stdout pipe를 non-blocking으로 만들고 기존 `poll()`에 등록
- request body를 CGI stdin으로 전달
- CGI stdout을 읽어 HTTP response로 변환
- CGI 환경 변수 구성
- `LocationConfig::cgi` 기반 CGI 라우팅/판별
- child process cleanup과 timeout 처리

이번 작업에서 CGI와 직접 연결되는 선행 수정도 같이 처리한다.

- accept 시점의 `setBodyLimit()` 제거
- `HttpParser::READING_BODY` 진입 후 server/location 매칭으로 body limit 계산
- handler dispatch를 `LocationConfig` 기반으로 바꾸기
- static path의 filesystem canonicalization 정책 정리

## 2. 현재 입력 계약

### HTTP parser 입력

Owner A의 `HttpRequest`에서 CGI가 사용할 값:

| 값 | 용도 |
| --- | --- |
| `method()` | `REQUEST_METHOD`, method dispatch |
| `path()` | script path, `SCRIPT_NAME`, `PATH_INFO` 계산 |
| `query()` | `QUERY_STRING` |
| `version()` | `SERVER_PROTOCOL` |
| `headers()` | `CONTENT_TYPE`, `HTTP_*` 환경 변수 |
| `body()` | CGI stdin으로 전달할 unchunked body |
| `getHost()` | `SERVER_NAME` |

Parser가 chunked request body를 unchunk해서 `body()`에 담는다는 계약은 그대로 사용한다.

### Config 입력

Owner B의 `LocationConfig`에서 CGI가 사용할 값:

| 값 | 용도 |
| --- | --- |
| `root` | script filesystem path 계산 |
| `methods` | 405 판단과 `Allow` header |
| `redirect` | CGI보다 먼저 처리 |
| `cgi` | extension -> interpreter 매핑 |
| `clientMaxBodySize` | body limit 주입 |

`Config::parse()` 이후 `LocationConfig::cgi`의 interpreter는 실행 가능한 절대 경로라고 가정한다.

## 3. 작업 순서

### Phase 1. Routing Context 만들기

현재 `EventLoop`는 `_server` 하나만 들고 `handleGet()` 내부에서 `matchLocation()`을 다시 호출한다. CGI와 body limit을 붙이려면 request 처리 전에 matched location이 확정되어야 한다.

작업:

1. `Connection`에 matched routing 정보를 저장한다.
   - `const ServerConfig* _server`
   - `const LocationConfig* _location`
   - 또는 single server skeleton을 유지한다면 최소 `const LocationConfig* _location`
2. `handleClientRead()`에서 `parser.feed()` 후 `READING_BODY`가 되면 routing을 수행한다.
3. matched location의 `clientMaxBodySize`로 `parser.setBodyLimit()`을 호출한다.
4. `COMPLETE` 이후 `handleRequest(request, server, location)` 형태로 넘긴다.

임시 single server 구조에서는 `_server`를 그대로 쓰되, 함수 시그니처는 최종 구조에 맞춘다.

```cpp
std::string handleRequest(const HttpRequest& request,
                          const ServerConfig& server,
                          const LocationConfig& location);
```

### Phase 2. Handler Dispatch 정리

CGI가 static보다 먼저 판단될 수 있도록 handler dispatch를 location 기반으로 정리한다.

권장 순서:

```text
match server
match location
redirect
method allowed check
CGI
POST upload
GET static
DELETE
405 fallback
```

작업:

1. `locationAllowsMethod(location, method)` 추가
2. `build405WithAllow(location)` 추가
3. `isRedirect(location)` / `handleRedirect(location)` 추가
4. `findCgiScript(request.path(), location, match)` 추가
5. CGI match가 있으면 `startCgi()`로 넘긴다.
6. CGI match가 없으면 기존 static/upload/delete로 dispatch한다.

## 4. CGI 판별 규칙

`LocationConfig::cgi`는 extension -> interpreter map이다.

예:

```nginx
location /cgi-bin {
    root ./www/cgi-bin;
    methods GET POST;
    cgi .py /usr/bin/python3;
}
```

요청:

```text
/cgi-bin/app.py/foo?x=1
```

판별 결과:

| 값 | 결과 |
| --- | --- |
| extension | `.py` |
| interpreter | `/usr/bin/python3` |
| script filename | `<root>/app.py` |
| script name | `/cgi-bin/app.py` |
| path info | `/foo` |
| query string | `x=1` |

판별 알고리즘:

1. `request.path()`가 `location.path`에 매칭된 상태라고 가정한다.
2. location prefix를 제거한 tail을 만든다.
3. tail을 path segment 단위로 왼쪽부터 누적한다.
4. 누적 path의 마지막 segment가 `location.cgi`의 extension 중 하나로 끝나면 script 후보로 본다.
5. 후보 script를 `location.root + 후보 tail`로 filesystem path로 만든다.
6. `realpath()`로 script canonical path를 얻는다.
7. `realpath(location.root)` 아래인지 확인한다.
8. regular file이고 read 가능해야 한다.
9. script 뒤 남은 tail은 `PATH_INFO`로 둔다.

주의:

- URL path normalization은 Owner A 결과를 믿는다.
- filesystem escape 방지는 server/handler 단계에서 `realpath()` root-prefix 검증으로 처리한다.
- `..` 문자열 자체를 금지하지 않는다. `file..txt`, `...` 같은 정상 이름을 막지 않기 위해서다.

## 5. CgiExecutor 설계

추가 파일:

```text
includes/Server/CgiExecutor.hpp
srcs/Server/CgiExecutor.cpp
```

초기 인터페이스:

```cpp
class CgiExecutor
{
public:
    enum State
    {
        NOT_STARTED,
        WRITING_STDIN,
        READING_STDOUT,
        FINISHED,
        FAILED
    };

    CgiExecutor();
    ~CgiExecutor();

    bool start(const HttpRequest& request,
               const ServerConfig& server,
               const LocationConfig& location,
               const CgiMatch& match);

    int inputFd() const;
    int outputFd() const;
    pid_t pid() const;
    State state() const;

    bool wantsInputWrite() const;
    bool wantsOutputRead() const;

    bool writeInput();
    bool readOutput();
    bool reapChild();
    bool timedOut(time_t now) const;
    void killChild();
    void closeInput();
    void closeOutput();

    std::string buildHttpResponse() const;
    int errorStatus() const;
};
```

`CgiMatch`는 script 판별 결과를 담는 작은 struct로 둔다.

```cpp
struct CgiMatch
{
    std::string extension;
    std::string interpreter;
    std::string scriptFilename;
    std::string scriptName;
    std::string pathInfo;
};
```

## 6. pipe/fork/execve 흐름

`CgiExecutor::start()` 흐름:

```text
pipe(stdinPipe)
pipe(stdoutPipe)
fork()

child:
    dup2(stdinPipe[0], STDIN_FILENO)
    dup2(stdoutPipe[1], STDOUT_FILENO)
    close all unused pipe ends
    build argv
    build envp
    execve(interpreter, argv, envp)
    _exit(1)

parent:
    close stdinPipe[0]
    close stdoutPipe[1]
    set stdinPipe[1] non-blocking
    set stdoutPipe[0] non-blocking
    store pid
    store request body and write offset
```

`argv`:

```text
argv[0] = interpreter
argv[1] = scriptFilename
argv[2] = NULL
```

현재 config는 extension마다 interpreter를 지정하는 구조이므로 direct exec 방식은 다루지 않는다.

## 7. CGI 환경 변수

최소 환경 변수:

| 이름 | 값 |
| --- | --- |
| `GATEWAY_INTERFACE` | `CGI/1.1` |
| `SERVER_PROTOCOL` | `request.version()` |
| `REQUEST_METHOD` | method 문자열 |
| `SCRIPT_FILENAME` | canonical script filesystem path |
| `SCRIPT_NAME` | URL상 script path |
| `PATH_INFO` | script 뒤 남은 path, 없으면 빈 문자열 |
| `QUERY_STRING` | `request.query()` |
| `CONTENT_LENGTH` | request body size, body 없으면 `0` 또는 빈 값 정책 결정 |
| `CONTENT_TYPE` | `Content-Type` header, 없으면 빈 문자열 |
| `SERVER_NAME` | `request.getHost()` 또는 matched server name |
| `SERVER_PORT` | listening port |
| `REMOTE_ADDR` | client address, 현재 accept에서 주소를 저장하도록 확장 필요 |
| `REDIRECT_STATUS` | `200`, PHP류 호환이 필요할 때 사용 |

HTTP headers 변환:

```text
User-Agent -> HTTP_USER_AGENT
Accept-Language -> HTTP_ACCEPT_LANGUAGE
```

변환 규칙:

1. header name을 uppercase로 바꾼다.
2. `-`를 `_`로 바꾼다.
3. `Content-Type`, `Content-Length`는 `HTTP_` prefix를 붙이지 않고 별도 변수로 둔다.
4. 나머지는 `HTTP_` prefix를 붙인다.

## 8. poll 통합

현재 `EventLoop`는 `_connections`만 보고 client fd를 등록한다. CGI fd를 추가하려면 fd ownership map이 필요하다.

추천 구조:

```cpp
std::map<int, int> _cgiInputToClient;
std::map<int, int> _cgiOutputToClient;
```

`buildPollFds()`:

```text
client READING  -> client fd POLLIN
client WRITING  -> client fd POLLOUT
CGI_WRITING     -> cgi input fd POLLOUT
CGI_READING     -> cgi output fd POLLIN
```

`handleReadyFd()`:

```text
if listen fd:
    accept
else if fd in _connections:
    client read/write
else if fd in _cgiInputToClient:
    write CGI stdin
else if fd in _cgiOutputToClient:
    read CGI stdout
else:
    close unknown fd defensively
```

`Connection::State` 확장:

```cpp
enum State
{
    READING,
    PROCESSING,
    CGI_WRITING,
    CGI_READING,
    WRITING,
    CLOSING
};
```

Connection에 추가할 값:

- matched server/location pointer 또는 index
- `CgiExecutor`
- response buffer/write offset 유지
- last activity time

## 9. CGI stdout 파싱

CGI stdout은 CGI response header와 body로 나눈다.

구분자:

```text
\r\n\r\n
```

호환을 위해 `\n\n`도 허용할지 결정한다. 추천은 둘 다 허용하되 최종 HTTP response는 `\r\n`으로 직렬화한다.

처리:

1. header block을 line 단위로 파싱한다.
2. `Status: 201 Created`가 있으면 status를 201로 설정한다.
3. `Content-Type` 등 일반 header를 response에 복사한다.
4. `Status` header 자체는 client에 보내지 않는다.
5. CGI body 길이로 `Content-Length`를 설정한다.
6. CGI output이 header 없이 body만 있으면 `200` + default content type 정책을 적용할지, `502`로 볼지 결정한다. 추천은 subject 디버깅 편의상 `502 Bad Gateway`.

오류 매핑:

| 상황 | status |
| --- | --- |
| fork/pipe 실패 | `500` |
| execve 실패 | `502` |
| CGI header malformed | `502` |
| CGI timeout | `504` |
| CGI child abnormal exit | `502` |

`statusReason()`에는 `201`, `204`, `301`, `302`, `303`, `307`, `308`, `414`, `431`, `502`, `504`를 추가한다.

## 10. Cleanup 규칙

client close 시:

- client fd close
- CGI stdin/stdout fd close
- child가 살아 있으면 kill
- `waitpid`로 회수
- fd map에서 제거

CGI 정상 종료 시:

- stdin을 모두 썼으면 input fd close
- stdout EOF를 받으면 output fd close
- child를 `waitpid(..., WNOHANG)`로 회수
- response 생성 후 client 상태를 `WRITING`으로 전환

timeout:

- CGI 시작 시간을 저장한다.
- 기본 timeout은 5초 또는 runtime policy와 별도 값으로 문서화한다.
- timeout이면 child kill 후 `504 Gateway Timeout`.

## 11. 구현 커밋 단위

추천 커밋 순서:

1. `refactor(server): store routing context before body parsing`
   - `setBodyLimit()` 타이밍 수정
   - `handleRequest(request, server, location)` 형태로 변경
2. `feat(server): add response helper for shared status handling`
   - status reason 확장
   - 405 Allow, redirect, CGI response 재사용 가능하게 정리
3. `feat(server): add CGI script matching`
   - `CgiMatch`
   - extension/interpreter 판별
   - `SCRIPT_NAME`, `PATH_INFO`, canonical path 검증
4. `feat(server): add CgiExecutor process startup`
   - pipe/fork/dup2/execve
   - envp/argv 구성
5. `feat(server): poll CGI pipes`
   - CGI stdin/stdout fd map
   - Connection state 확장
   - non-blocking write/read
6. `feat(server): parse CGI response`
   - Status/header/body 파싱
   - 502/504 처리
7. `test(server): cover CGI request flows`
   - GET CGI
   - POST CGI body
   - query string
   - malformed output
   - timeout

## 12. 테스트 계획

테스트용 script:

```python
#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()
print("Content-Type: text/plain")
print()
print("method=" + os.environ.get("REQUEST_METHOD", ""))
print("query=" + os.environ.get("QUERY_STRING", ""))
print("body=" + body)
```

필수 테스트:

- `GET /cgi-bin/echo.py?x=1`
- `POST /cgi-bin/echo.py` with Content-Length body
- chunked POST -> parser unchunk -> CGI stdin
- `/cgi-bin/echo.py/extra/path` -> `PATH_INFO=/extra/path`
- CGI script missing -> 404 또는 403 정책 확인
- interpreter exec 실패 -> 502
- malformed CGI header -> 502
- CGI timeout -> 504
- root 밖 symlink script 실행 차단
- method not allowed -> 405 + Allow

## 13. 완료 기준

CGI 작업 완료 기준:

- CGI request가 별도 blocking loop 없이 기존 `poll()` 안에서 처리된다.
- CGI stdin/stdout 모두 non-blocking fd로 관리된다.
- request body는 unchunked 상태로 CGI stdin에 전달된다.
- CGI stdout은 HTTP response로 변환되어 `Content-Length`가 붙는다.
- child process가 정상/비정상/timeout 모든 경로에서 회수된다.
- client disconnect 시 CGI child와 pipe fd가 정리된다.
- script path가 location root 밖으로 나가지 않는다.
- `make`가 통과한다.
- 위 테스트 계획의 기본 CGI GET/POST/query/PATH_INFO 케이스가 통과한다.
