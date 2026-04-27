# Validation Rules

이 문서는 파싱이 끝난 `Config` 객체에 대해 `ConfigValidator` 가 수행하는 의미 검증 규칙을 명세합니다. 모든 규칙은 위반 시 `ConfigError(VALIDATION, …)` 예외를 던지거나, 명시된 경우에만 경고 후 자동 보정합니다.

## Convention

- **MUST**: 위반 시 즉시 실패.
- **SHOULD**: 위반 시 warning 후 합리적 기본값으로 보정.
- **MAY**: 정보성 안내.

## Global

| ID | Rule | Severity |
| --- | --- | --- |
| V-G-1 | `Config::servers` 는 비어 있지 않아야 합니다. | MUST |
| V-G-2 | 한 conf 안의 모든 `(host, port)` 쌍 중 동일한 것이 있다면, 두 server 의 `serverNames` 가 서로 disjoint 해야 합니다. | MUST |
| V-G-3 | 동일 `(host, port)` 첫 server 가 default. 이후 server 는 `server_name` 매칭 전용. | MAY (정보) |

## Server-Level

| ID | Rule | Severity |
| --- | --- | --- |
| V-S-1 | `listen` 이 정의되어야 합니다. | MUST |
| V-S-2 | `port` 는 1..65535. | MUST |
| V-S-3 | `host` 는 IPv4 점-십진수, `localhost`, 또는 호스트명 형식. | MUST |
| V-S-4 | `serverNames` 의 각 이름은 `[A-Za-z0-9.-]+`. | MUST |
| V-S-5 | `clientMaxBodySize` 는 1..512MB (`0` 명시 시 그대로 0 으로 허용, 의미는 "0 바이트"). | MUST |
| V-S-6 | `errorPages` 의 키는 300..599. | MUST |
| V-S-7 | `errorPages` 의 value 경로는 `/` 로 시작. | MUST |
| V-S-8 | `locations` 가 비어 있으면 warning 후 자동으로 `location / { root ./www; }` 를 추가하지 **않습니다** (의도치 않은 fallback 방지). 단, warning 은 출력. | SHOULD |

## Location-Level

| ID | Rule | Severity |
| --- | --- | --- |
| V-L-1 | `path` 는 `/` 로 시작. | MUST |
| V-L-2 | `path` 끝 `/` 는 normalize 후 제거 (단, `/` 자체는 유지). | (자동 보정) |
| V-L-3 | 동일 server 안에 같은 `path` 의 location 이 둘 이상이면 마지막만 유지하고 warning. | SHOULD |
| V-L-4 | `root` 가 정의되어야 합니다. (단, `redirect` 가 설정된 경우 면제) | MUST |
| V-L-5 | `root` 디렉터리가 파일 시스템에 존재하고 read 권한이 있어야 합니다. | MUST |
| V-L-6 | `methods` 가 빈 셋이면 검증 실패. | MUST |
| V-L-7 | `methods` 는 `GET`, `POST`, `DELETE` 외 값을 가질 수 없습니다 (lexer/parser 에서 이미 차단됨, 이중 안전망). | MUST |
| V-L-8 | `index` 가 빈 문자열이면 기본값 `index.html` 으로 채움. | (자동 보정) |
| V-L-9 | `redirect.first` 가 0 이 아니면 301/302/303/307/308 중 하나여야 합니다. | MUST |
| V-L-10 | `redirect.second` 는 `/` 로 시작하거나 `http://` / `https://` 로 시작. | MUST |
| V-L-11 | `uploadStore` 가 비어 있지 않다면, 디렉터리가 존재하고 write 권한이 있어야 합니다. | MUST |
| V-L-12 | `uploadStore` 가 설정된 location 의 `methods` 에 `POST` 가 포함되어야 합니다. | MUST |
| V-L-13 | `cgi` 의 키는 `.` 으로 시작하는 확장자. | MUST |
| V-L-14 | `cgi` 의 value 인터프리터 경로는 절대 경로이며 실행 권한이 있어야 합니다. | MUST |
| V-L-15 | `clientMaxBodySize` 가 0 이면 server 의 값으로 fallback. | (자동 보정) |

## Filesystem Checks

런타임에 파일 시스템에 의존하는 검증은 다음과 같습니다.

| Check | When | API |
| --- | --- | --- |
| `root` 존재성과 read 권한 | V-L-5 | `stat` + `access(R_OK \| X_OK)` |
| `uploadStore` 존재성과 write 권한 | V-L-11 | `stat` + `access(W_OK \| X_OK)` |
| CGI 인터프리터 실행 권한 | V-L-14 | `stat` + `access(X_OK)` |

Linux 환경 가정이므로 `<unistd.h>`, `<sys/stat.h>` 사용.

## Defaulting Pass

검증 직후 다음 디폴트값 채우기를 한 번에 수행합니다.

1. `ServerConfig::host` 가 빈 문자열이면 `"0.0.0.0"`.
2. `ServerConfig::clientMaxBodySize` 가 0 이면 `1m` (`1048576`).
3. `LocationConfig::index` 가 빈 문자열이면 `"index.html"`.
4. `LocationConfig::methods` 가 빈 셋이면 `{ HTTP_GET }`. 단, V-L-6 에 의해 이미 실패했어야 함.
5. `LocationConfig::clientMaxBodySize` 가 0 이면 server 값을 복사.

`5.` 의 복사는 검증 단계에서 수행하지만, 이 fallback 의미를 다른 코드가 알지 못해도 되도록 location 값을 채워둡니다.

## Error Message Examples

```text
config/default.conf: validation error: server block has no listen directive
config/default.conf: validation error: location '/upload' has upload_store '/var/upload' but POST is not in methods
config/default.conf: validation error: cgi interpreter '/usr/bin/python4' is not executable
```

라인/컬럼 정보는 가능하면 lexer 단계에서 보존된 토큰 위치를 인용합니다. 검증 단계에서 위치를 복원할 수 없는 항목은 line/col 을 0 으로 두고 카테고리 + 메시지만 출력합니다.

## Test Coverage Targets

각 규칙은 [test-plan.md](test-plan.md) 의 케이스와 1:1 대응됩니다. 새 규칙을 추가할 때 반드시 테스트 케이스를 함께 추가합니다.
