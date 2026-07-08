#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build/check -DCMAKE_BUILD_TYPE=Debug -DOPTIFLOW_BUILD_SERVICES=ON
cmake --build build/check -j
ctest --test-dir build/check --output-on-failure
./build/check/apps/optiflow_cli/optiflow_cli >/tmp/optiflow_cli_output.csv
