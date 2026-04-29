# Error Page Policy

이 문서는 에러 페이지 매칭과 fallback HTML 생성 정책을 정의합니다. B 가 데이터(맵) 와 정책 명세를 제공하고, C 가 실제로 응답을 만들 때 이 정책을 따릅니다.

## Responsibilities

| Owner | Responsibility |
| --- | --- |
| B | `ServerConfig::errorPages` 데이터를 채워줍니다. 매칭 알고리즘과 fallback 명세를 이 문서에서 정의합니다. |
| C | 응답 생성 시점에 이 문서의 알고리즘을 따라 본문을 결정합니다. C 가 호출할 헬퍼 함수의 시그니처는 아래 [Helper Function](#helper-function) 절을 참조합니다. |
| A | 별도 책임 없음. 단, parser 가 status code 결정 후 에러 응답이 필요하면 C 의 헬퍼를 호출합니다. |

`LocationConfig` 에는 `errorPages` 가 존재하지 않습니다 (interface-agreement 합의). 따라서 location 별 에러 페이지 분기가 필요해지면 합의를 먼저 갱신해야 합니다.

## Lookup Order

응답 status code `S` 와 매칭된 server `Sv` 가 주어졌을 때 본문 결정 순서는 다음과 같습니다.

1. `Sv->errorPages` 에 `S` 가 있으면 그 경로를 사용.
2. 없으면 fallback HTML 자동 생성 (아래 [Fallback HTML](#fallback-html) 절).

매칭된 경로는 항상 `/` 로 시작합니다 ([validation.md](validation.md) V-S-7).

## Resolving Error Page Path

매칭된 경로 `P` 를 실제 디스크 파일로 해석하는 방법은 다음과 같습니다.

- `Sv` 의 첫 번째 location 중 `P` 와 가장 긴 prefix 매칭이 되는 location 을 찾습니다.
- 그 location 의 `root` + `P` 의 location prefix 이후 부분 = 디스크 경로.
- 예: `error_page 404 /errors/404.html;` 이고, `location /errors { root ./www; }` 이면 디스크 경로는 `./www/errors/404.html`.
- 매칭 location 이 없으면, 첫 번째 location 의 `root` 를 사용. 그것도 없으면 fallback HTML.

이 해석을 수행하는 책임은 C 에 있습니다. B 는 `errorPages` 맵에 절대 경로가 아니라 **URL-like 경로**를 저장합니다.

## Loop Prevention

에러 페이지 자체 응답 도중 또 다른 에러가 발생할 수 있습니다 (예: 404 페이지 파일이 없음). 다음 규칙으로 무한 루프를 방지합니다.

- 에러 페이지 파일이 존재하지 않으면 fallback HTML 로 즉시 fallthrough. 추가 lookup 금지.
- 에러 페이지 파일을 읽다가 IO 에러가 나면 동일하게 fallback HTML.
- fallback HTML 도 실패할 수 없도록 메모리 내 문자열로 직접 생성.

## Fallback HTML

`error_page` 지정이 없거나 위 단계 모두 실패 시, C 는 다음 형식의 HTML 을 즉석 생성합니다.

```html
<!DOCTYPE html>
<html>
<head><title>{CODE} {REASON}</title></head>
<body>
<center><h1>{CODE} {REASON}</h1></center>
<hr>
<center>webserv</center>
</body>
</html>
```

- `{CODE}` 는 status code (예: `404`).
- `{REASON}` 은 `statusReason(code)` 헬퍼의 반환값 (예: `Not Found`).
- 응답 헤더: `Content-Type: text/html; charset=UTF-8`, `Content-Length` 자동.

이 HTML 은 NGINX 의 기본 에러 페이지와 시각적으로 매우 유사합니다. 의도된 디자인입니다.

## Status Reason Strings

`const char* statusReason(int code)` 가 반환하는 문자열은 다음과 같습니다.

| Code | Reason |
| --- | --- |
| 200 | OK |
| 201 | Created |
| 204 | No Content |
| 301 | Moved Permanently |
| 302 | Found |
| 303 | See Other |
| 307 | Temporary Redirect |
| 308 | Permanent Redirect |
| 400 | Bad Request |
| 403 | Forbidden |
| 404 | Not Found |
| 405 | Method Not Allowed |
| 408 | Request Timeout |
| 411 | Length Required |
| 413 | Payload Too Large |
| 414 | URI Too Long |
| 500 | Internal Server Error |
| 501 | Not Implemented |
| 502 | Bad Gateway |
| 504 | Gateway Timeout |
| 505 | HTTP Version Not Supported |

이 표는 모든 팀이 동일한 위치(`StatusReason.cpp`) 에서 관리합니다. B 는 검증 중 status code 가 이 표에 없는 값이어도 fallback HTML 이 동작하도록 `"Error"` 같은 generic reason 을 사용합니다.

## Helper Function

C 가 호출할 헬퍼는 다음과 같이 정의합니다 (header 위치는 C 모듈 내부).

```cpp
// 단일 책임: status, server, location 으로부터 응답 body 와 Content-Type 결정.
// 파일 IO 실패 또는 매핑 실패 시 fallback HTML 사용.
struct ErrorBody {
    std::string body;
    std::string contentType;   // "text/html; charset=UTF-8"
};

ErrorBody buildErrorBody(int status,
                         const ServerConfig& server);
```

B 는 이 시그니처에 의존하는 코드를 작성하지 않습니다. 이 문서가 C 에게 정책 명세 역할만 합니다.

## Examples

### Example 1: server 단일 정의

```nginx
server {
    listen 8080;
    error_page 404 /errors/404.html;

    location / {
        root ./www;
    }

    location /api {
        root ./www;
    }
}
```

`/api/missing` 과 `/missing` 모두 404 → `/errors/404.html` 사용.

> location 별로 다른 404 를 주려면 현재로선 별도 server 블록을 두거나, 인터페이스 합의를 갱신해 `LocationConfig::errorPages` 를 도입해야 합니다.

### Example 2: 누락된 파일 → fallback

`error_page 500 /errors/500.html;` 인데 파일이 없으면 fallback HTML 사용.

### Example 3: error_page 미설정

server 안에 `error_page` 가 없으면 해당 server 의 모든 4xx/5xx 가 fallback HTML.
