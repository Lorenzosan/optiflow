#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build \
  -DOPTIFLOW_BUILD_CLI=ON \
  -DOPTIFLOW_BUILD_TESTS=ON \
  -DOPTIFLOW_BUILD_DOCS=OFF
cmake --build build -j
