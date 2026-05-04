# Server Architecture

이 문서는 현재 코드에 맞춘 C 파트 구조를 적는다. subject 전체 요구사항과 bonus 확장 기준은 `en.subject.pdf` 원문을 따른다.

## File Layout

현재 C 파트에서 직접 추가한 파일은 아래다.

```text
includes/
    Server/
        EventLoop.hpp
        Connection.hpp
        CgiExecutor.hpp
srcs/
    Server/
        EventLoop.cpp
        Connection.cpp
        CgiExecutor.cpp
srcs/
    main.cpp
```

Config 통합을 위해 현재 최소 parser 도 들어가 있다.

```text
includes/
    Config/
        Config.hpp
srcs/
    Config/
        Config.cpp
```

## Main Flow

`main` 은 config 경로를 받은 뒤 첫 번째 server 설정으로 event loop 를 실행한다.

```text
main
-> check argc == 2
-> Config::parse(argv[1])
-> config.servers[0]
-> EventLoop::run()
```

현재는 subject 의 default config path 선택지를 쓰지 않는다. 인자가 없으면 실패 종료한다.

## Listen Setup

`EventLoop::run()` 은 loop 시작 전에 listen socket 하나를 만든다.

```text
socket(AF_INET, SOCK_STREAM, 0)
setsockopt(SO_REUSEADDR)
fcntl(F_SETFL, O_NONBLOCK)
bind(INADDR_ANY, ServerConfig::port)
listen()
```

`sockaddr_in` 은 `bind()` 에 넘기는 주소 정보다. non-blocking 설정과는 별개다.

## Poll Loop

`poll()` 은 `EventLoop::run()` 안에 한 번만 있다.

```text
while true
    build pollfd list
    poll()
    for each ready fd
        handleReadyFd(fd)
```

`pollfd` 관심 이벤트는 현재 아래 fd들을 쓴다.

| FD | Condition | Event |
| --- | --- | --- |
| listen fd | 항상 등록 | `POLLIN` |
| client fd | request 읽는 중 | `POLLIN` |
| client fd | response 쓰는 중 | `POLLOUT` |
| CGI stdin pipe | CGI body 쓰는 중 | `POLLOUT` |
| CGI stdout pipe | CGI response 읽는 중 | `POLLIN` |

CGI pipe fd 는 별도 loop 를 만들지 않고 같은 `poll()` 목록에 넣는다.

CGI 구현 계획과 남은 런타임 완성 계획은 [cgi-implementation-plan.md](cgi-implementation-plan.md)에 둔다.

## Accept Flow

listen fd 가 `POLLIN` 이면 client 를 하나 accept 한다.

```text
accept()
fcntl(client, F_SETFL, O_NONBLOCK)
Connection 생성
remote address 저장
_connections[clientFd] = connection
```

`accept()` 실패는 현재 무시하고 다음 loop 로 넘긴다. subject 에서 금지한 `errno` 기반 분기는 하지 않는다.

body limit 은 accept 시점에 주입하지 않는다. request head parse 이후 matched location 이 확정되면 `LocationConfig::clientMaxBodySize` 를 parser 에 설정한다.

## Read Flow

client fd 가 `POLLIN` 이면 `recv()` 로 들어온 bytes 를 parser 에 넘긴다.

```text
recv()
HttpParser::feed(bytes)
if parser FAILED
    build error response
else if parser COMPLETE
    handle request
else
    keep reading
```

TCP 는 byte stream 이라서 `recv()` 한 번을 request 하나로 보지 않는다. request 완료 여부는 `HttpParser` 상태로만 판단한다.

## Request Handling

현재 request 처리 범위는 redirect, method allowed check, CGI, static GET 이다.

| Method | Current behavior |
| --- | --- |
| `GET` | CGI match 우선, 없으면 정적 파일 응답 |
| `POST` | CGI match 우선, upload 는 아직 `501 Not Implemented` |
| `DELETE` | 아직 `501 Not Implemented` |
| unknown | `501 Not Implemented` |

`GET` 흐름은 아래다.

```text
matchLocation(_server, request.path())
build file path from location.root and location.index
stat()
lexical normalized relative path 검증
reject target outside root by lexical containment
access(R_OK)
open/read/close regular file
build 200 response
```

URL path normalization 은 A parser 결과를 믿는다. subject PDF 허용 함수 목록에는 `realpath`, `lstat`, `readlink`, `openat`이 없으므로, C handler 단계는 lexical root containment 검증까지만 수행한다.

## Write Flow

client fd 가 `POLLOUT` 이면 준비된 response buffer 를 `send()` 한다.

```text
send(writeBuffer + writeOffset)
advance writeOffset
if all bytes sent
    close connection
```

`send()` 가 한 번에 전체 응답을 보낸다는 보장이 없으므로 `Connection` 이 write offset 을 가진다.

## Connection

`Connection` 은 client 하나의 parser 와 response 전송 상태를 가진다. fd 번호는 `_connections` map 의 key 이므로 객체 안에 다시 저장하지 않는다.

| Field | Use |
| --- | --- |
| `State` | `READING`, `CGI`, `WRITING` |
| `HttpParser` | partial request 상태 보관 |
| `CgiExecutor` | CGI child process 와 pipe 상태 |
| `_writeBuffer` | 직렬화된 response bytes |
| `_writeOffset` | 이미 전송한 byte 수 |

현재 keep-alive 는 구현하지 않는다. response 전송이 끝나면 connection 을 닫는다.

## Response

현재 별도 `HttpResponse` class 는 없다. `EventLoop` 안에서 문자열 response 를 만든다.

```text
HTTP/1.1 <status> <reason>
Connection: close
Content-Length: <body bytes>
Content-Type: <type>
```

`405` 응답에는 location methods 기준의 `Allow` header 를 추가한다.

## Current Limits

현재 구조에서 아직 subject 최종 조건이 아닌 부분은 아래 8 개다.

- `error_page` 적용
- POST upload
- DELETE
- autoindex directory listing
- multiple server / Host routing
- 여러 listen socket / bind host 반영
- keep-alive / pipelining
- transient non-blocking error 처리

Subject 허용 함수 준수 관점의 세부 계획은 [cgi-implementation-plan.md](cgi-implementation-plan.md)의 "Subject 허용 함수 제약" 섹션을 따른다.
