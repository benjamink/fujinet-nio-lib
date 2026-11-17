#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/tests/common-utility-cc65"
mkdir -p "$OUT"

# Run the same wire vectors with 16-bit int/size_t, not just hosted GCC.
# Large test fixtures are static only under cc65; codecs add no packet buffers.
for suite in file clock_format list_pages; do
    case "$suite" in
        file) sources="fn_file_common fn_file_stat fn_file_read fn_file_write fn_file_mkdir fn_disk_inspect fn_disk_common" ;;
        clock_format) sources="fn_clock_format_capacity fn_clock_format_common fn_clock_get_format fn_clock_get_tz" ;;
        list_pages) sources="fn_file_list_pages" ;;
    esac
    objects=()
    source_paths=("$ROOT/tests/${suite}_wire_test.c")
    for name in $sources; do source_paths+=("$ROOT/src/common/$name.c"); done
    for source in "${source_paths[@]}"; do
        object="$OUT/$(basename "${source%.c}").o"
        cl65 -t sim6502 -O -I"$ROOT/include" -c -o "$object" "$source"
        objects+=("$object")
    done
    cl65 -t sim6502 -m "$OUT/$suite.map" \
        -o "$OUT/$suite" "${objects[@]}"
    sim65 "$OUT/$suite"
done
