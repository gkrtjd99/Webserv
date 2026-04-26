# Sample Configuration Files

이 문서는 `config/` 디렉터리에 둘 샘플 conf 파일들을 어떤 의도로 작성하는지, 그리고 각 파일이 검증해야 하는 시나리오를 정리합니다.

## File Map

| File | Purpose | Tests It Backs |
| --- | --- | --- |
| `config/default.conf` | 골든 파일. 평가 시 기본 사용. 모든 디렉티브를 한 번씩 사용. | M1, M2 통합 테스트 |
| `config/examples/minimal.conf` | 최소한의 동작 확인용 (단일 server, GET 만). | smoke test |
| `config/examples/cgi.conf` | CGI 디렉티브가 동작하는지 검증. | CGI 통합 테스트 |
| `config/examples/upload.conf` | upload_store 디렉티브와 POST 가 함께 동작하는지 검증. | upload 통합 테스트 |
| `config/examples/multi-server.conf` | 동일 host:port 위 여러 server_name 분기. | 라우팅 통합 테스트 |
| `config/examples/redirect.conf` | `return` 디렉티브 동작 확인. | redirect 통합 테스트 |
| `config/examples/include-base.conf` + `include-extra.conf` | include 디렉티브 동작 확인. | include 통합 테스트 |

## Naming Convention

- 골든 파일은 항상 `default.conf`.
- 예시는 `examples/<feature>.conf`.
- 잘못된 conf 는 `tests/config/fixtures/bad/` 에 별도로 둡니다 (이 문서 범위 외, [test-plan.md](test-plan.md) 참조).

## Required Directories

각 sample conf 가 참조하는 디스크 경로는 미리 만들어 둡니다.

```text
www/
    index.html
    errors/
        404.html
        500.html
    cgi-bin/
        hello.py
        echo.py
    upload/
        .gitkeep
www/api/
    index.html
```

`www/` 가 비어 있어도 정적 GET 이 가능하도록 `www/index.html` 은 최소한 placeholder 라도 둡니다.

## default.conf (Specification)

다음 형태로 작성합니다. (실제 파일은 [implementation-roadmap.md](implementation-roadmap.md) 의 Step 2 에서 작성 예정)

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 1m;
    error_page 404 /errors/404.html;
    error_page 500 502 503 504 /errors/5xx.html;

    location / {
        root ./www;
        index index.html;
        autoindex on;
        methods GET POST DELETE;
    }

    location /upload {
        root ./www;
        methods POST;
        upload_store ./www/upload;
        client_max_body_size 5m;
    }

    location /cgi-bin {
        root ./www/cgi-bin;
        methods GET POST;
        cgi .py /usr/bin/python3;
    }

    location /redirect {
        return 301 /;
    }
}
```

이 파일이 검증하는 항목:

- 모든 12개 지원 디렉티브가 한 번씩 등장.
- server-level + location-level `client_max_body_size` 의 fallback 동작.
- 동일 `error_page` 의 다중 status code 지정.
- redirect 가 다른 location 의 `root` / `methods` 와 충돌하지 않음.

## minimal.conf (Specification)

```nginx
server {
    listen 8080;

    location / {
        root ./www;
    }
}
```

검증 항목: 가장 작은 합법 conf. `index`, `autoindex`, `methods` 가 모두 디폴트값으로 채워져야 합니다.

## cgi.conf (Specification)

```nginx
server {
    listen 8081;

    location /cgi-bin {
        root ./www/cgi-bin;
        methods GET POST;
        cgi .py /usr/bin/python3;
    }
}
```

검증 항목: cgi 인터프리터 권한 체크 (V-L-14). Linux 환경에서 `/usr/bin/python3` 은 평가 머신에 거의 항상 존재.

## upload.conf (Specification)

```nginx
server {
    listen 8082;
    client_max_body_size 10m;

    location /upload {
        root ./www;
        methods POST;
        upload_store ./www/upload;
    }
}
```

검증 항목: V-L-11 (디렉터리 존재성) + V-L-12 (POST 포함 확인) + server-level `client_max_body_size` 가 location 으로 전파.

## multi-server.conf (Specification)

```nginx
server {
    listen 8083;
    server_name a.local;

    location / { root ./www; }
}

server {
    listen 8083;
    server_name b.local;

    location / { root ./www/api; }
}
```

검증 항목: V-G-2 (동일 host:port + 다른 server_name 허용).

## redirect.conf (Specification)

```nginx
server {
    listen 8084;

    location / { root ./www; }
    location /old { return 301 /new; }
    location /external { return 302 https://example.com; }
}
```

검증 항목: V-L-9 와 V-L-10 의 두 종류 redirect 형식.

## include-base.conf + include-extra.conf (Specification)

`include-base.conf`:

```nginx
server {
    listen 8085;
    include examples/include-extra.conf;
}
```

`include-extra.conf`:

```nginx
location / {
    root ./www;
    methods GET;
}
```

검증 항목: server 안에서 location 만 들어 있는 partial conf 의 include 동작.

## What Is Intentionally NOT in Samples

- 잘못된 문법 예시 (별도 fixture 디렉터리에서 다룹니다).
- 비-Linux 경로 (`C:\…`).
- 환경변수 (지원 안 함).
- 현재 미지원 디렉티브 (`alias`, `try_files`, `keepalive_timeout`).
