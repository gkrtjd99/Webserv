# HTTP Runtime Policy

이 문서는 HTTP parser/request 설계를 넘어 `Connection`, `Response`, directory listing, error page, timeout 정책에 영향을 주는 런타임 합의 사항을 정리한다.

아래 내용은 팀 미팅에서 확정한 정책이다.

## 1. HTTP/1.1 지원 범위

HTTP/1.1은 `keep-alive`까지만 지원한다. HTTP pipelining은 지원하지 않는다.

- `Connection: keep-alive`와 `Connection: close`를 처리한다.
- 한 connection에서 응답 전송이 완료되기 전에는 다음 request를 처리하지 않는다.
- 다음 request는 이전 response write가 끝난 뒤 파싱을 시작한다.

### Connection 처리 정책

HTTP/1.1의 기본 연결 정책은 keep-alive로 본다.

```text
HTTP/1.1 + Connection 헤더 없음       -> keep-alive
HTTP/1.1 + Connection: keep-alive    -> keep-alive
HTTP/1.1 + Connection: close         -> response 전송 후 close
```

pipelining 요청처럼 하나의 read buffer에 여러 request가 들어오더라도, 현재 response가 완료되기 전에는 다음 request를 처리하지 않는다.

## 2. 응답 전송 방식

응답은 항상 `Content-Length`를 사용한다. chunked response는 사용하지 않는다.

- Response body는 buffer에 모두 모은 뒤 `Content-Length`를 계산한다.
- CGI 출력도 streaming으로 바로 보내지 않고 buffering한 뒤 `Content-Length`를 부여한다.
- `Transfer-Encoding: chunked`는 response에 사용하지 않는다.

### Response 처리 정책

`HttpResponse::serialize()` 또는 response 직렬화 계층은 body 길이를 기준으로 `Content-Length`를 확정해야 한다.

```http
HTTP/1.1 200 OK
Content-Length: 1234
Connection: keep-alive
```

body가 없는 응답도 `Content-Length: 0`을 명시하는 것을 기본으로 한다.

## 3. Directory Listing HTML 형식

Directory listing은 NGINX와 시각적으로 동일한 형식으로 생성한다.

- 첫 행은 부모 디렉터리(`../`)로 둔다.
- 현재 경로가 `/`이면 부모 디렉터리 행은 생략한다.
- 디렉터리는 이름 끝에 `/`를 붙인다.
- 파일은 이름을 그대로 표시한다.
- 컬럼은 이름, mtime, 크기 순서로 둔다.
- 디렉터리 크기는 `-`로 표기한다.
- `<pre>` 태그 안에서 공백으로 정렬한다.
- 정렬 순서는 디렉터리 먼저, 그 다음 파일이다.
- 각 그룹 안에서는 이름 알파벳 순으로 정렬한다.

### 출력 예시

```html
<html>
<head><title>Index of /upload/</title></head>
<body>
<h1>Index of /upload/</h1><hr><pre>
<a href="../">../</a>
<a href="images/">images/</a>                                           26-Apr-2026 12:00                   -
<a href="sample.txt">sample.txt</a>                                      26-Apr-2026 12:01                 123
</pre><hr>
</body>
</html>
```

정렬과 HTML escaping은 directory listing helper가 담당한다.

## 4. 기본 에러 페이지

기본 에러 페이지는 서버 내장 HTML 문자열로 생성한다.

- `error_page` 설정이 있으면 해당 파일을 우선 사용한다.
- `error_page` 설정이 없거나 파일을 사용할 수 없으면 내장 기본 HTML을 반환한다.
- 내장 HTML은 status code와 reason phrase만 포함하는 최소 형태로 둔다.

### 출력 예시

```html
<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body><h1>404 Not Found</h1></body>
</html>
```

reason phrase는 공통 status helper에서 가져온다.

## 5. Idle Connection Timeout

idle connection timeout 기본값은 30초다.

- 마지막 read 또는 write 이후 30초간 무활동이면 connection을 닫는다.
- keep-alive 상태에서 다음 request를 기다리는 동안에도 동일하게 적용한다.
- timeout 판정 기준은 `Connection::lastActivity()` 또는 동등한 timestamp다.

```text
now - connection.lastActivity() > 30
-> close connection
```

## 6. 최대 동시 연결 수

최대 동시 client connection 수는 256개로 제한한다.

- active client connection이 256개 이상이면 새 connection은 즉시 close한다.
- 기존 connection은 유지한다.
- listening socket과 CGI pipe fd는 client connection 수에 포함하지 않는다.

실제 file descriptor 한도는 OS 설정의 영향을 받으므로, 구현에서는 `accept` 이후 connection 등록 전에 현재 client connection 수를 확인한다.

## 7. 다음 작업 영향

| 대상 | 반영할 내용 |
| --- | --- |
| `Connection` | keep-alive 처리, `Connection: close` 처리, idle timeout, max connection 카운팅 |
| `HttpResponse` | buffering 기반 `Content-Length` 부여, chunked response 미사용 |
| Directory listing helper | NGINX 스타일 HTML 생성, 정렬, mtime/size 출력 |
| Error page helper | `error_page` config 조회, 내장 HTML fallback 생성 |
| CGI 통합 | CGI stdout을 buffering한 뒤 `Content-Length`를 계산해서 응답 |

이 정책은 `docs/HTTP/request-parser-design.md`의 parser/routing 정책과 함께 적용한다.
