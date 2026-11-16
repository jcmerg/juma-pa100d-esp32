#!/bin/sh
# Host tests for the status parser, the band mapping and the HEX reader of the
# PA firmware update - no ESP32 needed.
set -e
cd "$(dirname "$0")/.."
c++ -std=c++17 -Wall -I src tests/test_parse.cpp src/juma_status.cpp src/bands.cpp -o /tmp/juma-tests
/tmp/juma-tests
c++ -std=c++17 -Wall -I src tests/test_hex.cpp src/pa_hex.cpp -o /tmp/juma-tests-hex
exec /tmp/juma-tests-hex
