#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-demo"
OUT_DIR="${ROOT_DIR}/demo-output"

mkdir -p "${OUT_DIR}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j
ctest --test-dir "${BUILD_DIR}" --output-on-failure

"${BUILD_DIR}/hpblr_record" --output "${OUT_DIR}/sample.hpblr" --producers 4 --events 25000 --payload-size 64
"${BUILD_DIR}/hpblr_tool" inspect --input "${OUT_DIR}/sample.hpblr"
"${BUILD_DIR}/hpblr_tool" dump --input "${OUT_DIR}/sample.hpblr" --limit 3
"${BUILD_DIR}/hpblr_tool" export --input "${OUT_DIR}/sample.hpblr" --format csv --limit 100 --output "${OUT_DIR}/sample.csv"
"${BUILD_DIR}/hpblr_bench" --output "${OUT_DIR}/bench.hpblr" --report "${OUT_DIR}/bench_report.json" --events 100000 --payload-size 128 --producers 4

echo "Demo artifacts written to ${OUT_DIR}"
