# HTTP Module Design

이 디렉터리는 Owner A 범위의 HTTP 관련 설계를 따로 정리한다.

## 문서 목록

- `request-parser-design.md`: `HttpMethod`, `HttpRequest`, `HttpParser`, `matchServer`, `matchLocation` 설계

## 기준

- 기존 `docs/interface-agreement.md`는 공통 합의 문서로 유지한다.
- HTTP parser, request model, routing helper의 상세 설계는 이 디렉터리 아래에 둔다.
- 구현 중 세부 결정이 바뀌면 기존 합의 문서가 아니라 이 디렉터리의 문서를 갱신한다.
