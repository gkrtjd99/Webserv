# Directive Reference

이 문서는 `Webserv` 가 지원하는 **모든 설정 디렉티브** 를 한 페이지에 모아 명세합니다. 표기 규칙은 다음과 같습니다.

- **Context**: 디렉티브를 둘 수 있는 위치. `server`, `location`, `top` (파일 최상단).
- **Syntax**: 인자 형식. `<…>` 는 필수, `[…]` 는 선택, `…` 은 0개 이상 반복.
- **Default**: 명시되지 않았을 때의 값.
- **Required**: 해당 컨텍스트에서 필수인지.
- **Stored In**: 어느 구조체 필드에 저장되는지.
- **Validation**: 의미 검증 단계의 규칙. 자세한 사항은 [validation.md](validation.md).

## Top-Level

### include

| Field | Value |
| --- | --- |
| Context | top, server, location |
| Syntax | `include <path>;` |
| Default | — |
| Required | No |
| Stored In | (파서 내부, 결과에는 남지 않음) |

지정한 conf 파일을 현재 위치에 그대로 펼쳐서 파싱합니다. 자세한 해석 규칙은 [grammar.md](grammar.md#include-resolution).

## Server Block

### server

| Field | Value |
| --- | --- |
| Context | top |
| Syntax | `server { … }` |
| Required | 최소 1개 |

서브-디렉티브를 담는 컨테이너입니다. 같은 conf 안에 여러 개 둘 수 있습니다.

### listen

| Field | Value |
| --- | --- |
| Context | server |
| Syntax | `listen <port>;` 또는 `listen <host>:<port>;` |
| Default | — |
| Required | **Yes** |
| Stored In | `ServerConfig::host`, `ServerConfig::port` |

- `host` 생략 시 `0.0.0.0` 으로 간주.
- `host` 는 IPv4 점-십진수, `localhost`, 또는 `[A-Za-z0-9.-]+` 형식의 호스트명.
- `port` 는 1..65535.
- 같은 server 블록 안에 `listen` 은 1번만 허용.
- 동일 `(host, port)` 쌍이 여러 server 블록에 나오면 첫 번째가 default server, 나머지는 `server_name` 으로 구분 (NGINX 와 동일).

### server_name

| Field | Value |
| --- | --- |
| Context | server |
| Syntax | `server_name <name> [name …];` |
| Default | (빈 리스트) |
| Required | No |
| Stored In | `ServerConfig::serverNames` |

- 한 server 블록에 1개의 `server_name` 디렉티브만 허용 (이름 자체는 여러 개 가능).
- 와일드카드 미지원. 정확 매칭만 사용.

### error_page

| Field | Value |
| --- | --- |
| Context | server |
| Syntax | `error_page <code> [code …] <path>;` |
| Default | (빈 맵) |
| Required | No |
| Stored In | `ServerConfig::errorPages` |

- `code` 는 300..599 의 정수.
- `path` 는 server 의 가장 가까운 root 기준의 URL-like 경로 (예: `/errors/404.html`).
- 같은 server 안에 같은 code 가 두 번 이상 등장하면 마지막 값이 적용.
- location 안에서의 `error_page` 는 현재 인터페이스 합의에 포함되지 않습니다. 추후 확장 시 [interface-agreement.md](../interface-agreement.md) 와 [architecture.md](architecture.md) 의 `LocationConfig` 정의에 `errorPages` 필드를 추가해야 합니다.
- 자세한 매칭 규칙은 [error-pages.md](error-pages.md).

### client_max_body_size

| Field | Value |
| --- | --- |
| Context | server, location |
| Syntax | `client_max_body_size <size>;` |
| Default | `1m` |
| Required | No |
| Stored In | `ServerConfig::clientMaxBodySize` 또는 `LocationConfig::clientMaxBodySize` |

- `size` 는 정수에 선택적으로 `k` 또는 `m` 접미사. 대소문자 구분 없음.
- `0` 은 "제한 없음" 이 아니라 "0 바이트만 허용" 으로 해석. NGINX 와 다름.
- 최댓값: `512m`. 그보다 크면 검증 실패.
- 인터페이스 합의대로, location 의 값이 `0` 이면 server 값으로 fallback. 즉 location 에서 명시적으로 `0` 은 무의미하므로 0 은 "미설정" 신호로 사용.

### location

| Field | Value |
| --- | --- |
| Context | server |
| Syntax | `location <path> { … }` |
| Required | 최소 1개 권장 |
| Stored In | `ServerConfig::locations` |

- `path` 는 `/` 로 시작하는 prefix 문자열.
- 한 server 안에 여러 location 가능. 동일 path 가 두 번 나오면 마지막이 우선.
- 매칭은 가장 긴 prefix 우선 (A 의 `matchLocation` 책임).

## Location Block

### root

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `root <path>;` |
| Default | — |
| Required | **Yes** |
| Stored In | `LocationConfig::root` |

- 디스크 상의 디렉터리 경로.
- 절대 경로 또는 conf 파일 기준 상대 경로.
- 끝에 `/` 가 있으면 제거하여 normalize.

### index

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `index <name> [name …];` |
| Default | `index.html` |
| Required | No |
| Stored In | `LocationConfig::index` |

- 인터페이스 합의에 따라 단일 문자열로 저장 (`std::string`). 여러 개를 받을 경우 첫 번째만 사용하고 나머지는 무시 (warning 로그).
- 향후 확장 시 `std::vector<std::string>` 으로 바꿀 수 있도록 표기.

### autoindex

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `autoindex on;` 또는 `autoindex off;` |
| Default | `off` |
| Required | No |
| Stored In | `LocationConfig::autoindex` |

### methods

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `methods <method> [method …];` |
| Default | `GET` 만 허용 |
| Required | No |
| Stored In | `LocationConfig::methods` |

- 허용 토큰: `GET`, `POST`, `DELETE`. 대문자만.
- 중복 토큰은 무시.
- 여기 명시되지 않은 메서드 요청은 405 응답.

### return

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `return <code> <target>;` |
| Default | — |
| Required | No |
| Stored In | `LocationConfig::redirect = (code, target)` |

- `code` 는 301, 302, 303, 307, 308 중 하나.
- `target` 은 절대 URL (`http://…`) 이거나 `/` 로 시작하는 경로.
- `return` 이 설정된 location 은 다른 디렉티브(예: `root`, `methods`)와 함께 있어도 redirect 가 우선.

### upload_store

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `upload_store <path>;` |
| Default | — |
| Required | upload 핸들러를 사용하려는 location 에서만 필요 |
| Stored In | `LocationConfig::uploadStore` |

- 업로드 저장 디렉터리.
- 디렉터리가 존재하지 않거나 쓰기 불가하면 검증 실패.
- 절대 경로 권장. 상대 경로는 conf 파일 기준.

### cgi

| Field | Value |
| --- | --- |
| Context | location |
| Syntax | `cgi <ext> <interpreter>;` |
| Default | (빈 맵) |
| Required | No |
| Stored In | `LocationConfig::cgi` |

- `ext` 는 `.` 로 시작하는 확장자 (예: `.py`).
- `interpreter` 는 실행 가능한 절대 경로.
- 같은 location 안에서 같은 확장자가 두 번 나오면 마지막 값이 우선.
- Linux 기준이므로 `interpreter` 의 존재성과 실행 권한을 검증 단계에서 확인.

## Defaults Summary

명시되지 않았을 때 적용되는 기본값을 한 곳에 모았습니다.

| Field | Default |
| --- | --- |
| `ServerConfig::host` | `0.0.0.0` |
| `ServerConfig::port` | (필수, 기본 없음) |
| `ServerConfig::serverNames` | `[]` |
| `ServerConfig::errorPages` | `{}` |
| `ServerConfig::clientMaxBodySize` | `1m` (`1048576` 바이트) |
| `LocationConfig::root` | (필수, 기본 없음) |
| `LocationConfig::index` | `"index.html"` |
| `LocationConfig::autoindex` | `false` |
| `LocationConfig::methods` | `{ GET }` |
| `LocationConfig::redirect` | `(0, "")` (미설정 의미) |
| `LocationConfig::uploadStore` | `""` |
| `LocationConfig::cgi` | `{}` |
| `LocationConfig::clientMaxBodySize` | `0` (server fallback 의미) |
