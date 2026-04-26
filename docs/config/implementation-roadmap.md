# Config Implementation Roadmap

이 문서는 B 가 수행할 작업을 PR 단위로 분할한 로드맵입니다. 각 단계는 collaboration-guide 의 권장 PR 크기(300줄 이하) 안에 들어와야 합니다.

## Milestones

| Milestone | Goal | Definition of Done |
| --- | --- | --- |
| **M0** | Stub PR 머지 | `Config::parse` 가 하드코딩된 1-server 설정을 반환. main 빌드 가능. |
| **M1** | 정적 GET 통합용 conf | `default.conf` 의 server/location 섹션을 진짜로 파싱. 검증 일부 동작. |
| **M2** | 전 기능 conf | `error_page`, CGI, upload, redirect, include 모두 동작. |

전체 일정 가정: 1주차 M0/M1 진입, 2주차 M2 마무리. 정확한 날짜는 팀 합의에서 정합니다.

## PR Plan

각 PR 의 제목은 Conventional Commits 규칙을 따릅니다. 예: `feat(config): add ConfigLexer`.

### PR-1 — `chore(config): scaffold module skeleton`

- `include/Config.hpp`, `include/ServerConfig.hpp`, `include/LocationConfig.hpp`, `include/ConfigError.hpp` 헤더 골격.
- `src/config/Config.cpp` 에 `Config::parse` 의 하드코딩 stub:
    - 단일 server, port 8080, 단일 location `/` with `root ./www`.
- `Makefile` 갱신: 새 src 파일 자동 인식 (이미 있는 경우 no-op).
- 테스트 없음. 단순 stub.
- 예상 변경량: 100줄 미만.
- M0 완료.

### PR-2 — `feat(config): add ConfigError type`

- `ConfigError` 클래스 구현 + format helper.
- `tests/config/test_error.cpp` 단위 테스트 1~2개.
- 예상 변경량: 80줄.

### PR-3 — `feat(config): add ConfigLexer`

- `ConfigLexer.hpp/cpp` 구현.
- `tests/config/test_lexer.cpp` 케이스 T-LEX-1..T-LEX-6.
- include 처리 없음 (다음 PR 에서).
- 예상 변경량: 250줄.

### PR-4 — `feat(config): add ConfigParser core`

- server/location 블록과 단일 디렉티브 처리.
- include 처리 미포함.
- `tests/config/test_parser.cpp` T-PARSE-1..T-PARSE-6.
- 예상 변경량: 400줄. 너무 크면 server 전용 PR 과 location 전용 PR 로 분할.

### PR-5 — `feat(config): include directive`

- `parseInclude` 추가 + 깊이/순환 검증.
- T-PARSE-7..T-PARSE-9 케이스.
- 예상 변경량: 150줄.

### PR-6 — `feat(config): add ConfigValidator`

- 모든 V-* 규칙 구현. 각 규칙은 별도 함수.
- defaulting pass 구현.
- `tests/config/test_validator.cpp` T-VAL-1..T-VAL-15.
- 예상 변경량: 400줄. 너무 크면 server-level / location-level / filesystem-checks 세 PR 로 분할.

### PR-7 — `feat(config): wire validator into Config::parse`

- `Config::parse` 가 lexer → parser → validator 순서로 호출.
- stub 코드 삭제.
- 통합 테스트 T-INT-1..T-INT-4.
- M1 완료.

### PR-8 — `chore(config): golden default.conf and examples`

- `config/default.conf` + `config/examples/*.conf` 작성.
- `www/` placeholder 파일 생성.
- 통합 테스트가 이 파일들을 참조하도록 갱신.
- 예상 변경량: 200줄 + conf 파일.

### PR-9 — `docs(config): error page policy and helper signature`

- C 가 이 문서를 보고 헬퍼를 만들 수 있도록 [error-pages.md](error-pages.md) 의 helper signature 합의 PR.
- 코드 변경 없음. 문서만.

### PR-10 — `feat(config): tighten validation messages`

- 에러 메시지 표준 형식으로 통일.
- 메시지 substring 매칭 테스트 강화.
- M2 직전 정리.

### PR-11 (선택) — `feat(config): support # line comments`

- Open Item G-1 결정 후, 채택 시 진행.
- 채택 안 하면 PR 폐기.

## Branch Names

| PR | Branch |
| --- | --- |
| PR-1 | `chore/config-scaffold` |
| PR-2 | `feature/config-error-type` |
| PR-3 | `feature/config-lexer` |
| PR-4 | `feature/config-parser-core` |
| PR-5 | `feature/config-include` |
| PR-6 | `feature/config-validator` |
| PR-7 | `feature/config-wire-validator` |
| PR-8 | `chore/config-golden-files` |
| PR-9 | `docs/config-error-page-helper` |
| PR-10 | `refactor/config-error-messages` |
| PR-11 | `feature/config-comments` |

## Dependencies

- PR-1 ← (없음)
- PR-2 ← PR-1
- PR-3 ← PR-2
- PR-4 ← PR-3
- PR-5 ← PR-4
- PR-6 ← PR-4 (PR-5 와 병렬 가능)
- PR-7 ← PR-5, PR-6
- PR-8 ← PR-7 (또는 PR-7 과 병렬)
- PR-9 ← (independent, 시작 가능)
- PR-10 ← PR-7

## Risk Register

| Risk | Mitigation |
| --- | --- |
| Validator 크기가 너무 커져 PR 분할 필요 | 처음부터 server/location/fs 3 분할 가능성을 염두에 두고 코드 정리 |
| include 처리에서 파일 경로 normalize 불일치 | Linux 만 가정, `realpath` 또는 manual normalize 함수 단위 테스트 우선 작성 |
| 에러 메시지가 자주 바뀌어 테스트 깨짐 | substring 매칭 + 메시지 표준 형식 PR-10 에서 고정 |
| C++98 기능 제한 위반 | `auto`, range-for 사용 금지 셀프 리뷰 체크리스트 PR 템플릿에 추가 |
| C 모듈과 error page helper 시그니처 불일치 | PR-9 를 먼저 합의 후 코드 진행 |

## Checklist Before Implementation Starts

- [ ] 모든 docs/config/*.md 가 main 에 머지됨
- [ ] interface-agreement.md 와 directives.md 의 디폴트값 일치 확인 ([directives.md Defaults Summary](directives.md#defaults-summary))
- [ ] A 와 C 가 본 문서를 한 번씩 읽고 충돌 없음 확인
- [ ] 합의된 PR 순서가 GitHub 이슈로 등록됨
- [ ] CI 또는 최소한 로컬 `make` 가 깨끗한 상태

위 체크리스트가 모두 통과되면 PR-1 부터 순차적으로 시작합니다.
