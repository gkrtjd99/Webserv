#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
OUTPUT_FILE="${1:-$ROOT_DIR/compile_commands.json}"
CXX_BIN="${CXX:-c++}"
CXX_FLAGS="${CXXFLAGS:--Wall -Wextra -Werror -std=c++98}"
CPP_FLAGS="${CPPFLAGS:--I$ROOT_DIR/includes -I$ROOT_DIR/includes/HTTP}"

json_escape()
{
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

SOURCE_LIST=$(find "$ROOT_DIR/srcs" -type f -name '*.cpp' | sort)

{
    printf '[\n'
    first=1
    for source_file in $SOURCE_LIST; do
        if [ "$first" -eq 0 ]; then
            printf ',\n'
        fi
        first=0

        abs_source=$(CDPATH= cd -- "$(dirname -- "$source_file")" && pwd -P)/$(basename -- "$source_file")
        command="$CXX_BIN $CXX_FLAGS $CPP_FLAGS -c $abs_source -o /tmp/$(basename -- "$source_file").o"

        printf '  {\n'
        printf '    "directory": "%s",\n' "$(json_escape "$ROOT_DIR")"
        printf '    "command": "%s",\n' "$(json_escape "$command")"
        printf '    "file": "%s"\n' "$(json_escape "$abs_source")"
        printf '  }'
    done
    printf '\n]\n'
} > "$OUTPUT_FILE"

printf 'generated %s\n' "$OUTPUT_FILE"
