# Config Grammar

이 문서는 `Webserv` 의 설정 파일 어휘 규칙과 문법을 EBNF 형식으로 정의합니다. 파서는 이 명세를 따라야 하며, 어떤 입력에 대해 성공하는지/실패하는지의 단일 진실 공급원입니다.

## File Encoding and Whitespace

- 인코딩: ASCII. 비-ASCII 바이트가 발견되면 즉시 파싱 실패.
- 줄 종료: `\n` 만 표준으로 취급합니다. `\r\n` 입력은 `\r` 을 공백처럼 무시.
- 들여쓰기: 의미 없음. 탭과 공백이 혼용되어도 동일하게 취급.
- 빈 줄: 무시.

## Comments

현재 합의: **주석 미지원**.

해당 결정은 다음 사유로 재검토를 권장합니다.

- `default.conf` 골든 파일과 examples/ 변형들에 의도 설명을 적기 어렵습니다.
- 평가 시 평가자가 conf 를 수정하면서 임시로 라인을 비활성화하기 불편합니다.

재검토 결정 시에는 NGINX 와 동일하게 `#` 부터 줄 끝까지를 주석으로 처리하는 안을 기본으로 채택합니다. 이 항목은 [Open Items](#open-items) 에 추적합니다.

## Tokens

| Token | Pattern | Notes |
| --- | --- | --- |
| `LBRACE` | `{` | 블록 시작 |
| `RBRACE` | `}` | 블록 종료 |
| `SEMI` | `;` | 단순 디렉티브 종결 |
| `IDENT` | `[A-Za-z_][A-Za-z0-9_]*` | 디렉티브 이름과 일부 인자 |
| `NUMBER` | `[0-9]+` | 정수 |
| `SIZE` | `[0-9]+[kKmM]?` | `client_max_body_size` 전용 |
| `STRING` | `"…"` 또는 비공백 토큰 | 따옴표 없는 토큰은 공백/`{`/`}`/`;` 까지 |
| `EOF` | (입력 종료) | |

문자열 토큰은 다음 규칙을 따릅니다.

- 따옴표 없는 토큰: 첫 비공백 문자에서 시작해 `;`, `{`, `}`, 공백 직전까지가 한 토큰.
- 큰따옴표 토큰: `"` 안의 모든 바이트는 리터럴. 이스케이프는 `\"` 와 `\\` 만 인정.
- 작은따옴표는 미지원.

## EBNF Grammar

```ebnf
config        = { top_directive } EOF ;

top_directive = include_dir
              | server_block ;

include_dir   = "include" path_token ";" ;

server_block  = "server" "{" { server_dir } "}" ;

server_dir    = listen_dir
              | server_name_dir
              | error_page_dir
              | client_max_body_size_dir
              | location_block
              | include_dir ;

listen_dir              = "listen" listen_value ";" ;
server_name_dir         = "server_name" name { name } ";" ;
error_page_dir          = "error_page" status_code { status_code } path_token ";" ;
client_max_body_size_dir = "client_max_body_size" SIZE ";" ;

location_block = "location" path_token "{" { location_dir } "}" ;

location_dir   = root_dir
               | index_dir
               | autoindex_dir
               | methods_dir
               | return_dir
               | upload_store_dir
               | cgi_dir
               | client_max_body_size_dir
               | include_dir ;
(* error_page 는 server_dir 에서만 허용. LocationConfig 에 errorPages 필드가 없음. *)

root_dir          = "root" path_token ";" ;
index_dir         = "index" name { name } ";" ;
autoindex_dir     = "autoindex" ("on" | "off") ";" ;
methods_dir       = "methods" method { method } ";" ;
return_dir        = "return" status_code path_token ";" ;
upload_store_dir  = "upload_store" path_token ";" ;
cgi_dir           = "cgi" extension path_token ";" ;

listen_value = port
             | host ":" port ;

host        = ipv4 | "localhost" | hostname ;
port        = NUMBER ;             (* 1..65535 *)
status_code = NUMBER ;             (* 300..599 *)
method      = "GET" | "POST" | "DELETE" ;
extension   = "." IDENT ;          (* 예: .py, .php *)
ipv4        = octet "." octet "." octet "." octet ;
octet       = NUMBER ;             (* 0..255 *)
hostname    = IDENT { "." IDENT } ;
name        = STRING | IDENT ;
path_token  = STRING ;             (* 슬래시 포함 가능 *)
```

`location_block` 은 중첩을 허용하지 않습니다 (서브젝트 범위 외).

## Include Resolution

- `include` 는 `top_directive` 와 `server_dir`, `location_dir` 어디서든 사용 가능.
- 인자는 단일 path_token 만 허용. 와일드카드 미지원.
- 경로 해석 규칙:
    - 절대 경로(`/` 로 시작): 그대로 사용.
    - 상대 경로: 현재 파일이 있는 디렉터리 기준으로 해석.
- 순환 include 검출: 파서가 파일 경로를 normalize 후 스택에 push, 이미 스택에 있으면 에러.
- include 깊이 제한: 8 단계. 초과 시 에러.
- include 된 파일도 동일한 어휘/문법 규칙을 따릅니다.

## Reserved Words

다음은 디렉티브 이름이거나 값으로만 사용 가능합니다.

`server`, `location`, `listen`, `server_name`, `error_page`, `client_max_body_size`, `root`, `index`, `autoindex`, `methods`, `return`, `upload_store`, `cgi`, `include`, `on`, `off`, `GET`, `POST`, `DELETE`.

위 단어를 디렉티브 이름이 아닌 위치에 따옴표 없이 적어도 일반 문자열 토큰으로 처리합니다 (단, 의미 검증 단계에서 거부될 수 있음).

## Lexical Errors

다음은 어휘 단계에서 즉시 실패시키는 케이스입니다.

| Case | Behavior |
| --- | --- |
| 비-ASCII 바이트 | `LexError`, line/col 보고 |
| 닫히지 않은 큰따옴표 | `LexError`, 시작 line/col 보고 |
| `\` 다음 허용되지 않은 이스케이프 | `LexError` |
| EOF 직전 토큰이 미완성 | `LexError` |

## Syntax Errors

문법 단계에서 다음을 검사합니다.

| Case | Status |
| --- | --- |
| 디렉티브 끝 `;` 누락 | 실패 |
| 블록 닫는 `}` 누락 | 실패 |
| `server` 외 top-level 디렉티브 (단, `include` 제외) | 실패 |
| 알 수 없는 디렉티브 이름 | 실패 |
| 디렉티브 인자 개수 불일치 | 실패 |
| `location` 의 path 가 누락 | 실패 |
| 동일 server 블록 안 동일 directive 가 중복 (예: `root` 두 번) | 실패. 단, `error_page`, `location`, `include` 는 중복 허용 |

## Error Reporting Format

파서가 던지는 예외는 다음 형식을 따릅니다.

```text
<file>:<line>:<col>: <category>: <message>
```

예시:

```text
config/default.conf:42:9: syntax error: expected ';' before '}'
config/default.conf:7:1: lex error: unterminated string starting here
config/default.conf:15:13: syntax error: unknown directive 'foo'
```

`category` 는 `lex error` 또는 `syntax error` 둘 중 하나입니다. 의미 검증 에러는 `validation error` 카테고리로 [validation.md](validation.md) 에서 다룹니다.

## Examples (Accepted)

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 1m;

    location / {
        root ./www;
        index index.html;
        autoindex on;
        methods GET POST DELETE;
    }
}
```

```nginx
server {
    listen 127.0.0.1:9000;
    server_name api.local;

    location /api {
        root ./www/api;
        methods GET POST;
        cgi .py /usr/bin/python3;
    }

    location /redirect {
        return 301 /api/;
    }
}
```

## Examples (Rejected)

```nginx
server {
    listen 8080
    location / { root ./www; }
}
```

이유: `listen` 끝에 `;` 누락.

```nginx
location / {
    root ./www;
}
```

이유: `server` 블록 밖에 `location` 사용.

```nginx
server {
    listen 70000;
    location / { root ./www; }
}
```

이유: 포트 범위 초과 (의미 검증 단계에서 실패).

## Open Items

| ID | Item | Status |
| --- | --- | --- |
| G-1 | `#` 라인 주석 도입 여부 재검토 | 재검토 권장 |
| G-2 | `include` 와일드카드 (`*.conf`) 지원 여부 | 현재 미지원, 도입 가능성 낮음 |
| G-3 | 작은따옴표 문자열 도입 여부 | 도입 안 함, 결정 |
