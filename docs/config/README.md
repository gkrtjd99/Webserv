# Config Module Documentation

이 디렉터리는 `Webserv` 의 **B 파트 — Config parser, 설정 검증, 샘플 conf, 에러 페이지 정책** 의 설계 문서를 담습니다. 구현 시작 전에 이 문서들을 모두 작성하여, 구현 단계에서는 합의된 명세를 따라 코딩만 하도록 준비하는 것이 목적입니다.

## Owner

- B (Config 담당)
- 대상 플랫폼: Linux (개발과 평가 모두 Linux 환경 가정)

## Scope

`docs/interface-agreement.md` 의 Work Split 표에서 B 가 맡기로 한 영역입니다.

- Config 파일 어휘 분석과 문법 분석
- 의미 검증 (중복 listen, 필수 필드, 값 범위 등)
- `Config`, `ServerConfig`, `LocationConfig` 데이터 구조 채우기
- 샘플 conf (`config/default.conf`) 작성과 유지
- 에러 페이지 정책: 매칭 규칙과 fallback HTML 생성 명세

다음은 B 의 책임이 **아닙니다**.

- 라우팅 매칭 함수 `matchServer`, `matchLocation` 의 구현 (A)
- HTTP 요청 처리 (A, C)
- Socket / event loop (C)
- Response 직렬화 (C)
- 정적 파일 서빙, CGI, upload 의 핸들러 코드 (A, C)

B 는 위 영역에서 사용할 **데이터 모델과 정책만 제공**합니다.

## Document Index

| File | Purpose |
| --- | --- |
| [grammar.md](grammar.md) | Config 파일 어휘 규칙과 EBNF 문법 |
| [directives.md](directives.md) | 지원 디렉티브 12개 각각의 명세 |
| [architecture.md](architecture.md) | 파서 클래스 구성, 데이터 흐름, public API |
| [validation.md](validation.md) | 파싱 후 의미 검증 규칙 목록 |
| [error-pages.md](error-pages.md) | error_page 매칭과 fallback HTML 생성 정책 |
| [sample-conf.md](sample-conf.md) | `config/` 하위 골든 파일과 예시 설명 |
| [test-plan.md](test-plan.md) | 단위/통합 테스트 케이스 목록 |
| [implementation-roadmap.md](implementation-roadmap.md) | 단계별 PR 분할과 마일스톤 |

## Pre-Approved Decisions

아래 결정은 회의에서 승인되었으며, 변경 시 팀 합의가 필요합니다.

- **디렉티브 범위**: 서브젝트 필수 12개로 한정합니다. (목록은 [directives.md](directives.md))
- **`client_max_body_size` 단위**: 정수와 `k`, `m` 접미사만 허용합니다. `g` 는 미지원.
- **기본 에러 페이지**: 사용자가 `error_page` 를 지정하지 않은 status 에 대해서는 C 가 내장 HTML 을 즉석 생성합니다. B 는 `errorPages` 맵이 비어 있을 때의 fallback 동작만 문서화합니다.
- **include 디렉티브**: 다른 conf 파일을 include 가능합니다. 자세한 처리 규칙은 [grammar.md](grammar.md) 참조.
- **주석**: 현재 결정은 `include` 만 선택되어 있으며 `#` 주석은 미지원입니다. 단, 이 결정은 실용성 측면에서 **재검토를 권장**합니다. ([grammar.md](grammar.md) 의 Open Items 참고)

## Out of Scope (B 가 다루지 않음)

- HTTP 메서드 핸들러
- Listening socket 의 실제 `bind`, `listen` 호출
- 파일 시스템 권한 검사 (런타임 시 C 의 핸들러가 처리)

## References

- [docs/collaboration-guide.md](../collaboration-guide.md)
- [docs/interface-agreement.md](../interface-agreement.md)
- [NGINX Configuration Reference](https://nginx.org/en/docs/dirindex.html)
