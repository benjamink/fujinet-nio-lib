#!/usr/bin/env bash
set -euo pipefail
soft65c02_unit -b build/tests/parse-read-c -i tests/atari_parse_read_reference_soft65c02.yaml
soft65c02_unit -b build/tests/parse-read-atari -i tests/atari_parse_read_soft65c02.yaml
