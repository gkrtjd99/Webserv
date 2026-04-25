# Collaboration Guide

이 문서는 3명이 `Webserv` 프로젝트를 진행할 때 사용할 협업 규칙입니다. 목표는 각자 병렬로 개발하되, `main` 브랜치는 항상 빌드 가능한 상태로 유지하는 것입니다.

## Principles

- 작은 단위로 나누어 작업합니다. 한 PR은 하나의 목적만 가져야 합니다.
- 기능 구현, 리팩터링, 테스트 추가, 문서 수정은 가능하면 서로 다른 PR로 분리합니다.
- `main` 브랜치에는 직접 푸시하지 않습니다.
- 리뷰를 기다리는 동안 다른 독립 작업을 진행할 수 있도록 작업을 작게 쪼갭니다.
- 논쟁이 길어지면 코드 리뷰 댓글보다 짧은 회의나 페어 프로그래밍으로 결정합니다.

## Team Roles

고정 역할보다 주 단위 순환 역할을 권장합니다.

| Role | Responsibility |
| --- | --- |
| Integrator | PR 머지 순서, 충돌 해결, `main` 빌드 상태를 확인합니다. |
| Reviewer | 리뷰 응답 시간을 관리하고, PR이 너무 커지지 않도록 조정합니다. |
| Tester | 수동 테스트 시나리오, `curl` 테스트, 회귀 테스트 체크리스트를 관리합니다. |

역할은 매주 회의에서 바꿉니다. 한 사람이 특정 영역을 계속 독점하지 않도록 합니다.

## Work Breakdown

초기에는 아래 영역으로 나누는 것이 좋습니다.

| Area | Main Tasks | Conflict Risk |
| --- | --- | --- |
| HTTP parser | 요청 라인, 헤더, 바디, chunked 처리 | 높음 |
| Server event loop | socket, bind, listen, accept, non-blocking I/O, poll/kqueue/epoll/select | 높음 |
| Configuration | config 파일 문법, server/location 설정, 기본값 검증 | 중간 |
| Response and routing | 정적 파일, directory listing, error page, redirect, method 제한 | 중간 |
| CGI and upload | CGI 실행, 환경변수, stdin/stdout 연결, 파일 업로드 | 높음 |
| Tests and fixtures | config 샘플, curl 스크립트, integration test | 낮음 |

3명이 동시에 시작할 때는 아래 순서를 권장합니다.

1. 공통 인터페이스 먼저 합의

    `Request`, `Response`, `Config`, `Server`, `Route`, `CgiExecutor` 같은 핵심 타입의 책임과 public 메서드를 짧게 합의합니다.

2. 충돌이 적은 세 갈래로 분리

    - A: HTTP parser와 Request 모델
    - B: Config parser와 설정 검증
    - C: Server socket/event loop 뼈대와 간단한 Response 출력

3. 통합 이후 기능 단위로 재분배

    - Static file과 directory listing
    - Method별 처리: GET, POST, DELETE
    - CGI와 upload
    - Error handling과 timeout

## Branch Strategy

작업 브랜치는 짧게 유지합니다.

```text
main
feature/<issue-number>-<short-topic>
fix/<issue-number>-<short-topic>
docs/<short-topic>
test/<short-topic>
refactor/<short-topic>
```

예시는 아래와 같습니다.

```text
feature/12-http-parser
fix/18-delete-status-code
docs/collaboration-guide
test/cgi-timeout
```

규칙은 아래와 같습니다.

- 브랜치는 이슈 하나 또는 작업 하나에 대응합니다.
- 하루 이상 작업한 브랜치는 매일 `main`을 반영합니다.
- 충돌 가능성이 큰 파일을 수정하기 전에 팀 채팅에 먼저 알립니다.
- `main`으로 머지하기 전 `make`와 필요한 테스트를 통과시킵니다.

## Commit Rules

커밋 메시지는 Conventional Commits 형식을 사용합니다.

```text
<type>(optional-scope): <description>
```

사용할 type은 아래로 제한합니다.

| Type | Use Case |
| --- | --- |
| feat | 사용자 관점의 새 기능 |
| fix | 버그 수정 |
| refactor | 동작 변경 없는 구조 개선 |
| test | 테스트 추가 또는 수정 |
| docs | 문서 변경 |
| build | Makefile, 빌드 옵션 변경 |
| chore | 기능과 직접 관련 없는 관리 작업 |

예시는 아래와 같습니다.

```text
feat(parser): parse request headers
fix(cgi): close unused pipe descriptors
test(config): add duplicate listen port case
docs: add collaboration guide
```

한 커밋에 여러 목적을 섞지 않습니다. 커밋 제목에 `and`를 넣고 싶어지면 커밋을 나눕니다.

## Pull Request Rules

PR은 작고 검토 가능한 상태로 만듭니다.

- 권장 크기: 300줄 이하 변경
- 최대 크기: 500줄 내외 변경
- 예외: 자동 포맷, 파일 이동, 대량 테스트 fixture 추가
- PR 제목은 커밋 메시지처럼 작성합니다.
- PR 설명에는 목적, 변경 내용, 테스트 결과, 리뷰어가 먼저 볼 파일을 적습니다.
- Draft PR은 설계 공유나 조기 피드백용으로 사용합니다.

PR 템플릿은 아래 형식을 권장합니다.

```markdown
## Summary

- 

## Test

- [ ] make
- [ ] 직접 실행 테스트
- [ ] curl 또는 telnet 테스트

## Review Notes

- 먼저 볼 파일:
- 의논이 필요한 부분:
```

## Review Rules

리뷰어는 아래 순서로 봅니다.

1. 요구사항과 동작이 맞는지 확인합니다.
2. 오류 처리, 리소스 해제, 파일 디스크립터 누수, timeout을 확인합니다.
3. C++98 호환성과 프로젝트 코딩 규칙을 확인합니다.
4. 테스트가 충분한지 확인합니다.
5. 이름, 중복, 구조 개선은 마지막에 봅니다.

리뷰 댓글은 아래 기준으로 구분합니다.

| Prefix | Meaning |
| --- | --- |
| `must` | 머지 전에 반드시 수정해야 합니다. |
| `suggestion` | 개선 제안입니다. 작성자가 선택할 수 있습니다. |
| `question` | 의도 확인 질문입니다. |
| `nit` | 사소한 스타일 의견입니다. |

작성자는 리뷰를 받기 전에 자신의 PR diff를 먼저 읽고, 빌드와 테스트 결과를 PR에 남깁니다.

## Communication Rules

매일 짧게 아래 세 가지를 공유합니다.

- 어제 완료한 것
- 오늘 할 것
- 막힌 것 또는 충돌 위험이 있는 파일

결정 사항은 채팅에만 두지 말고 이슈, PR, 문서 중 하나에 남깁니다. 특히 parser와 config 문법처럼 나중에 바꾸기 어려운 결정은 문서화합니다.

## Definition Of Done

작업은 아래 조건을 만족해야 완료입니다.

- 요구사항이 구현되었습니다.
- `make`가 성공합니다.
- 관련 테스트 또는 수동 검증이 수행되었습니다.
- 에러 경로가 확인되었습니다.
- 리소스 해제 규칙이 확인되었습니다.
- PR 설명에 테스트 결과가 기록되었습니다.
- 최소 1명 이상의 승인을 받았습니다.

## Project TODO

프로젝트 초기에 아래 TODO를 이슈로 만들어 관리합니다.

- [ ] 핵심 클래스 책임과 public 인터페이스 합의
- [ ] config 파일 예제와 문법 범위 합의
- [ ] HTTP request parser 최소 동작 구현
- [ ] socket 초기화와 event loop 뼈대 구현
- [ ] 정적 파일 응답 구현
- [ ] error page와 status code 처리 구현
- [ ] GET, POST, DELETE method 처리 구현
- [ ] CGI 실행과 timeout 처리 구현
- [ ] upload 저장 정책 구현
- [ ] curl 기반 수동 테스트 목록 작성
- [ ] 평가 전 전체 회귀 테스트 체크리스트 작성

## Recommended Workflow

1. 이슈를 만듭니다.
2. 작업 범위를 1일 이내 PR 크기로 줄입니다.
3. 브랜치를 만듭니다.
4. 작게 커밋합니다.
5. Draft PR을 일찍 올립니다.
6. `make`와 필요한 테스트를 실행합니다.
7. Ready for review로 바꾸고 리뷰어를 지정합니다.
8. 리뷰 반영 후 squash merge 또는 rebase merge로 `main`에 반영합니다.
9. 머지 후 팀 채팅에 완료와 다음 작업을 공유합니다.

## References

- [GitHub Docs: Helping others review your changes](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/getting-started/helping-others-review-your-changes)
- [Google Engineering Practices: Small CLs](https://google.github.io/eng-practices/review/developer/small-cls.html)
- [Atlassian: A Guide to Optimal Branching Strategies in Git](https://www.atlassian.com/agile/software-development/branching)
- [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)
