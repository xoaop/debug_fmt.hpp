#!/usr/bin/env bash
# Build and run the smoke test.
set -e
cmake --build build
./build/debug_fmt_test
