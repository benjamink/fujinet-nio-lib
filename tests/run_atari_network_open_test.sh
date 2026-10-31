#!/usr/bin/env bash
set -euo pipefail
soft65c02_unit -b build/tests/network-open-c -i tests/atari_network_open_reference_soft65c02.yaml
soft65c02_unit -b build/tests/network-open-atari -i tests/atari_network_open_soft65c02.yaml
