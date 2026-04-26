# Config Test Plan

이 문서는 B 모듈의 자동/수동 테스트 케이스 전체 목록을 정리합니다. 각 케이스는 한 가지 규칙 또는 동작만 검증하도록 작성합니다.

## Test Layers

| Layer | What | How |
| --- | --- | --- |
| Lexer 단위 | 토큰 분리, lex error | 작은 inline 문자열 입력 |
| Parser 단위 | 문법 매칭, syntax error | fixture 파일 + 직접 호출 |
| Validator 단위 | 의미 검증 통과/실패 | parser 가 만든 Config 를 직접 주입 |
| 통합 | `Config::parse(path)` 한 번으로 전체 흐름 | `tests/config/fixtures/` |
| 수동 | curl 로 확인 가능한 시나리오 | `docs/config/test-plan.md` 의 [Manual](#manual-test-checklist) 절 |

테스트 프레임워크는 외부 의존을 피하기 위해 가벼운 자체 구현을 사용합니다 (assert + main). 또는 팀이 합의하면 `catch2 single header` 도입을 검토하지만, 현재 결정은 **자체 구현**.

## Fixtures Layout

```text
tests/config/fixtures/
    good/
        minimal.conf
        full.conf
        cgi.conf
        upload.conf
        multi-server.conf
        redirect.conf
        include-base.conf
        include-extra.conf
    bad/
        lex/
            non_ascii.conf
            unterminated_string.conf
        syntax/
            missing_semicolon.conf
            unknown_directive.conf
            location_outside_server.conf
            duplicate_root.conf
            unmatched_brace.conf
        validation/
            no_listen.conf
            port_out_of_range.conf
            cgi_bad_interpreter.conf
            upload_no_post.conf
            same_host_port_same_name.conf
            empty_methods.conf
            ...
```

`good/` 는 일부가 [sample-conf.md](sample-conf.md) 의 파일과 동일합니다. 단순히 `config/` 의 파일을 심볼릭 링크 또는 복사로 둡니다.

## Test Case Catalog

각 케이스는 ID, 입력, 기대 결과 형식으로 적습니다. ID 는 `T-<layer>-<n>`.

### Lexer

| ID | Input | Expected |
| --- | --- | --- |
| T-LEX-1 | `server { listen 8080; }` | 6개 토큰 + EOF |
| T-LEX-2 | `"a b c"` | 단일 STRING 토큰 `a b c` |
| T-LEX-3 | `"abc` | LexError, line 1, col 1 |
| T-LEX-4 | 파일 안에 `\xC3\xA9` (é) | LexError (비 ASCII) |
| T-LEX-5 | `#hello server{}` | STRING `#hello`, IDENT `server`, LBRACE, RBRACE, EOF |
| T-LEX-6 | `server{listen 8080;}` (공백 없음) | 정상 토큰화 |

T-LEX-5 의 의미: 주석 미지원 결정 하에서 `#` 은 어휘적으로 평범한 unquoted 토큰의 일부일 뿐입니다. 따라서 lexer 단계에서는 LexError 가 아니라 STRING 토큰으로 흘러가고, 이후 parser 가 "알 수 없는 디렉티브 '#hello'" 같은 SyntaxError 를 발생시킵니다. Open Item G-1 (`#` 주석 도입) 가 채택되면 본 케이스의 기대 결과는 "토큰 0개 + EOF" 로 갱신됩니다.

### Parser

| ID | Input | Expected |
| --- | --- | --- |
| T-PARSE-1 | minimal.conf | `Config` 1 server, 1 location, 디폴트 채워짐 |
| T-PARSE-2 | missing_semicolon.conf | SyntaxError, 정확한 line/col |
| T-PARSE-3 | unknown_directive.conf | SyntaxError "unknown directive 'foo'" |
| T-PARSE-4 | location_outside_server.conf | SyntaxError |
| T-PARSE-5 | duplicate_root.conf | SyntaxError "directive 'root' duplicated" |
| T-PARSE-6 | unmatched_brace.conf | SyntaxError, `}` 누락 |
| T-PARSE-7 | include-base.conf + include-extra.conf | 두 파일이 합쳐진 단일 Config |
| T-PARSE-8 | 자기 자신을 include 하는 conf | SyntaxError (cycle) |
| T-PARSE-9 | include 깊이 9 단계 | SyntaxError (depth) |

### Validator

| ID | Input Config | Expected |
| --- | --- | --- |
| T-VAL-1 | server 없음 | ValidationError V-G-1 |
| T-VAL-2 | listen 없음 | ValidationError V-S-1 |
| T-VAL-3 | port 70000 | ValidationError V-S-2 |
| T-VAL-4 | clientMaxBodySize 600m | ValidationError V-S-5 |
| T-VAL-5 | error_page code 999 | ValidationError V-S-6 |
| T-VAL-6 | location path "abc" (`/` 없음) | ValidationError V-L-1 |
| T-VAL-7 | location root 누락, redirect 없음 | ValidationError V-L-4 |
| T-VAL-8 | location root 가 존재하지 않는 경로 | ValidationError V-L-5 |
| T-VAL-9 | upload_store 있고 POST 없음 | ValidationError V-L-12 |
| T-VAL-10 | cgi 인터프리터 권한 없음 | ValidationError V-L-14 |
| T-VAL-11 | 같은 host:port 같은 server_name 두 server | ValidationError V-G-2 |
| T-VAL-12 | location clientMaxBodySize 0 → server 값 fallback | 자동 보정 (예외 없음) |
| T-VAL-13 | location index 빈 문자열 → "index.html" 보정 | 자동 보정 |
| T-VAL-14 | redirect code 200 (잘못된 redirect code) | ValidationError V-L-9 |
| T-VAL-15 | redirect target "missing-leading-slash" | ValidationError V-L-10 |

### Integration

| ID | Input | Expected |
| --- | --- | --- |
| T-INT-1 | `Config::parse("config/default.conf")` | 예외 없음, 모든 디렉티브 채워짐 |
| T-INT-2 | `Config::parse("config/examples/cgi.conf")` | 예외 없음 |
| T-INT-3 | `Config::parse("does_not_exist.conf")` | 예외, 파일 IO 실패 메시지 |
| T-INT-4 | `Config::parse("tests/config/fixtures/bad/syntax/missing_semicolon.conf")` | SyntaxError |

## Manual Test Checklist

자동 테스트로는 잡기 어려운 점검 항목입니다. M1, M2 마일스톤 직전에 실행합니다.

- [ ] `./webserv config/default.conf` 가 비-0 종료 없이 시작.
- [ ] 잘못된 conf 파일을 인자로 줬을 때, stderr 에 한 줄짜리 진단 + 비-0 종료.
- [ ] include 경로 해석이 cwd 와 무관 (다른 디렉터리에서 실행해도 동작).
- [ ] `ulimit -n 32` 같은 낮은 fd 한도에서도 conf 파싱이 정상 종료 (파싱 단계가 fd 를 누수하지 않음).
- [ ] valgrind --leak-check=full 로 `Config::parse` 만 호출하는 미니 main 실행, leak 0.

## What This Plan Does Not Cover

- HTTP 응답 검증 (A, C 의 책임).
- 성능/벤치마크.
- fuzz 테스트 (시간 여유 있을 때 별도 PR).

## Maintenance Rules

- 새 디렉티브 추가 시 [directives.md](directives.md), [validation.md](validation.md), 본 문서에 모두 케이스를 추가.
- fixture 파일은 한 케이스가 한 가지만 검증하도록 작게 유지.
- 실패 메시지 문구를 테스트가 부분 매칭(substring) 으로 검사하여, 메시지 미세 변경에 깨지지 않게 합니다.
