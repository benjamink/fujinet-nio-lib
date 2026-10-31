#!/usr/bin/env bash
set -euo pipefail
soft65c02_unit -b build/tests/network-write-c -i tests/atari_network_write_reference_soft65c02.yaml
soft65c02_unit -b build/tests/network-write-atari -i tests/atari_network_write_soft65c02.yaml
