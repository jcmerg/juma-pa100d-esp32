#!/bin/sh
# Host tests for the status parser and band mapping - no ESP32 needed.
set -e
cd "$(dirname "$0")/.."
c++ -std=c++17 -Wall -I src tests/test_parse.cpp src/juma_status.cpp src/bands.cpp -o /tmp/juma-tests
exec /tmp/juma-tests
