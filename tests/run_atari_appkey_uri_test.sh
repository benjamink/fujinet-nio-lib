#!/usr/bin/env bash
set -euo pipefail
soft65c02_unit -b build/tests/appkey-uri-c -i tests/atari_appkey_uri_reference_soft65c02.yaml
soft65c02_unit -b build/tests/appkey-uri-atari -i tests/atari_appkey_uri_soft65c02.yaml
