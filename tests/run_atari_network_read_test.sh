#!/usr/bin/env bash
set -euo pipefail
soft65c02_unit -b build/tests/network-read-c -i tests/atari_network_read_reference_soft65c02.yaml
soft65c02_unit -b build/tests/network-read-atari -i tests/atari_network_read_soft65c02.yaml
