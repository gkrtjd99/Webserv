# Config Module Architecture

이 문서는 B 가 구현할 config 모듈의 내부 구성, 클래스 책임, 데이터 흐름, 그리고 외부에 노출하는 public API 를 정의합니다.

## Goals and Non-Goals

**Goals**

- `Config::parse(path)` 한 줄로 사용 가능해야 합니다.
- 어휘/문법/의미 에러를 동일한 형식으로 보고합니다.
- 다른 모듈(A 의 router, C 의 event loop)이 데이터 모델만 보면 되도록 합니다.
- include 와 다중 server, 다중 location 을 안정적으로 다룹니다.
- C++98 호환, 외부 라이브러리 없음.

**Non-Goals**

- 런타임 reload 미지원 (서브젝트 범위 외).
- 환경변수 치환 미지원.
- conf 안에서의 변수 정의 미지원.

## Layered Design

config 모듈은 4 개 레이어로 나눕니다.

```text
+-----------------------------+
|        Config (DTO)         |   <- 다른 모듈이 사용하는 결과
+--------------+--------------+
               ^
               |
+--------------+--------------+
|         Validator           |   <- 의미 검증
+--------------+--------------+
               ^
               |
+--------------+--------------+
|          Parser             |   <- 토큰 -> AST -> DTO
+--------------+--------------+
               ^
               |
+--------------+--------------+
|           Lexer             |   <- 바이트 -> 토큰
+-----------------------------+
```

각 레이어는 하나의 .hpp/.cpp 쌍으로 구현합니다.

## File Layout

```text
include/
    Config.hpp
    ServerConfig.hpp
    LocationConfig.hpp
    ConfigLexer.hpp
    ConfigParser.hpp
    ConfigValidator.hpp
    ConfigError.hpp
src/
    config/
        Config.cpp
        ConfigLexer.cpp
        ConfigParser.cpp
        ConfigValidator.cpp
        ConfigError.cpp
config/
    default.conf
    examples/
        minimal.conf
        cgi.conf
        upload.conf
        multi-server.conf
tests/
    config/
        test_lexer.cpp
        test_parser.cpp
        test_validator.cpp
        fixtures/
            ...
```

`include/` 와 `src/config/` 의 1:1 대응 규칙을 따릅니다 ([interface-agreement.md](../interface-agreement.md) Project Layout 참조).

## Public API

다른 모듈이 포함하는 헤더는 `Config.hpp` 단 하나입니다. 나머지는 config 모듈 내부 구현 상세입니다.

### Config.hpp

```cpp
#ifndef WEBSERV_CONFIG_CONFIG_HPP
#define WEBSERV_CONFIG_CONFIG_HPP

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "HttpMethod.hpp"   // 공용 enum

struct LocationConfig {
    std::string                          path;
    std::string                          root;
    std::string                          index;
    bool                                 autoindex;
    std::set<HttpMethod>                 methods;
    std::pair<int, std::string>          redirect;       // (code, target)
    std::string                          uploadStore;
    std::map<std::string, std::string>   cgi;            // ext -> interpreter
    std::size_t                          clientMaxBodySize;  // 0 = server fallback
};
// 주의: errorPages 는 ServerConfig 에만 존재합니다 (interface-agreement 합의).
//       location-level error_page 가 필요해지면 합의를 먼저 갱신해야 합니다.

struct ServerConfig {
    std::string                       host;
    int                               port;
    std::vector<std::string>          serverNames;
    std::map<int, std::string>        errorPages;
    std::size_t                       clientMaxBodySize;
    std::vector<LocationConfig>       locations;
};

struct Config {
    std::vector<ServerConfig> servers;

    static Config parse(const std::string& path);
};

#endif
```

`Config::parse` 는 다음 단계로 동작합니다.

1. `ConfigLexer` 로 토큰 스트림 생성.
2. `ConfigParser` 로 AST/DTO 채우기.
3. `ConfigValidator` 로 의미 검증.
4. 검증 통과 시 채워진 `Config` 반환. 실패 시 `ConfigError` 예외.

## Internal Components

### ConfigError.hpp

```cpp
#ifndef WEBSERV_CONFIG_CONFIG_ERROR_HPP
#define WEBSERV_CONFIG_CONFIG_ERROR_HPP

#include <stdexcept>
#include <string>

class ConfigError : public std::runtime_error {
public:
    enum Category { LEX, SYNTAX, VALIDATION };

    ConfigError(Category cat,
                const std::string& file,
                int line,
                int col,
                const std::string& msg);

    Category    category() const;
    const std::string& file() const;
    int         line() const;
    int         col() const;

private:
    Category     cat_;
    std::string  file_;
    int          line_;
    int          col_;
};

#endif
```

- `what()` 은 [grammar.md](grammar.md#error-reporting-format) 의 표준 형식 문자열을 반환.
- `cat`, `file`, `line`, `col` 은 테스트에서 검증할 때 사용.

### ConfigLexer.hpp

```cpp
class ConfigLexer {
public:
    enum TokenType {
        TOKEN_IDENT,
        TOKEN_STRING,
        TOKEN_NUMBER,
        TOKEN_LBRACE,
        TOKEN_RBRACE,
        TOKEN_SEMI,
        TOKEN_EOF
    };

    struct Token {
        TokenType    type;
        std::string  value;
        std::string  file;
        int          line;
        int          col;
    };

    explicit ConfigLexer(const std::string& source,
                         const std::string& file);

    Token next();
    Token peek();

private:
    // ... 내부 필드 ...
};
```

- 입력은 이미 메모리에 로드된 한 파일의 내용. include 처리는 `ConfigParser` 가 담당하며 lexer 는 한 파일만 다룹니다.
- `next()` 는 EOF 이후에도 안전하게 EOF 토큰을 반복 반환.

### ConfigParser.hpp

```cpp
class ConfigParser {
public:
    explicit ConfigParser(const std::string& rootPath);
    Config parse();

private:
    Config              result_;
    std::string         rootPath_;
    std::vector<std::string> includeStack_;

    void parseTop();
    void parseServerBlock();
    void parseLocationBlock(ServerConfig& server);
    void parseInclude();

    // ... 디렉티브 처리 헬퍼들 ...
};
```

- 파일 IO 와 include 스택은 parser 가 관리합니다.
- 한 파일을 lex 하면서 만들어진 토큰을 즉시 소비합니다.
- 결과는 `result_` 에 누적되며, `parse()` 가 반환.

### ConfigValidator.hpp

```cpp
class ConfigValidator {
public:
    explicit ConfigValidator(Config& cfg);
    void run();    // 실패 시 ConfigError 예외
};
```

- 검증 항목은 [validation.md](validation.md) 의 표를 그대로 구현합니다.
- 검증은 in-place 로 동작하지만 디폴트값 채우기 같은 mutation 만 허용하고, 사용자 입력값을 임의로 바꾸지는 않습니다.

## Data Flow

```text
file path
   |
   v
[ read file to string ] --(include 만남)--> [ recurse ]
   |
   v
[ ConfigLexer ] --tokens--> [ ConfigParser ]
                                  |
                                  v
                         [ Config (raw) ]
                                  |
                                  v
                         [ ConfigValidator ]
                                  |
                                  v
                         [ Config (validated) ]
                                  |
                                  v
                          return to caller
```

## Error Propagation

- 모든 단계는 실패 시 `ConfigError` 예외를 던집니다.
- 호출자(예: `main`) 는 한 군데에서 `try/catch` 로 받아 stderr 로 출력 후 비-0 종료합니다.

```cpp
int main(int argc, char** argv) {
    try {
        Config cfg = Config::parse(argv[1]);
        EventLoop loop(cfg);
        loop.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
```

## Memory and Ownership

- 모든 데이터는 값 타입 (`std::string`, `std::vector`, `std::map`, `std::set`).
- 포인터/참조는 모듈 외부로 전달되지 않습니다.
- `Config` 는 복사 가능하지만 비용이 큽니다. EventLoop 는 한 번만 받아 보관합니다.

## Threading

- 단일 스레드. 동시성 고려 없음. config 객체는 `const` 로 공유됩니다.

## Logging

- 검증 단계의 경고(warning) 는 `LOG_WARN` 매크로로 stderr 에 기록.
- 예: `index` 디렉티브에 두 개 이상의 인자가 들어오면 첫 번째만 사용 + warning.
- 파싱 단계는 로그를 남기지 않습니다 (성공 시 조용).

## Testing Hooks

- `ConfigLexer`, `ConfigParser`, `ConfigValidator` 는 모두 단독 테스트가 가능합니다.
- 단위 테스트는 `tests/config/` 의 fixture 파일을 읽어 실행합니다. 자세한 사항은 [test-plan.md](test-plan.md).

## Open Questions for Architecture

| ID | Item | Status |
| --- | --- | --- |
| A-1 | `ConfigParser` 가 lexer 를 직접 소유 vs 토큰 벡터를 받기 | 직접 소유로 결정 (스트리밍, 메모리 절약) |
| A-2 | `Config` 를 `EventLoop` 가 복사 vs 참조 보관 | 참조 (EventLoop 가 받은 시점 소유권 이전, const&) |
| A-3 | 디폴트값을 validator 가 채울지, parser 가 채울지 | validator 단계에서 일괄 처리로 통일 |
