# cpp-high-performance-binary-log-recorder

A C++20 high-performance binary event recorder using producer-consumer queues, background writing, replay tools and benchmark reports.

This project implements a small software black box: multiple producers generate technical events, a bounded thread-safe queue centralizes them, and a background writer serializes them to an integrity-checked binary file. Companion CLI tools can inspect, dump, filter, export and benchmark the recorder.

## Why this project is useful

It demonstrates production-oriented C++ skills around:

- modern C++20 RAII, `std::jthread`, move-aware queues and exception-safe APIs;
- Linux CLI tooling and binary file I/O;
- producer-consumer concurrency with `std::mutex`, `std::condition_variable` and `std::atomic`;
- compact binary serialization with explicit little-endian encoding;
- CRC-based file and record validation;
- robust replay, filtering and CSV/JSON export;
- repeatable benchmark reports;
- automated tests through CMake and CTest.

## Repository layout

```text
.
├── apps/                  # CLI applications: record, tool, bench
├── docs/                  # Technical documentation in English
├── include/hpblr/         # Public C++ headers
├── scripts/               # Demo helper scripts
├── src/core/              # Core library implementation
└── tests/                 # Unit and integration-style tests
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

For a stricter build:

```bash
cmake -S . -B build-werror -DCMAKE_BUILD_TYPE=Release -DHPBLR_WARNINGS_AS_ERRORS=ON
cmake --build build-werror -j
ctest --test-dir build-werror --output-on-failure
```

## Quick start

Generate a binary log:

```bash
./build/hpblr_record --output sample.hpblr --producers 4 --events 50000 --payload-size 64
```

Inspect it:

```bash
./build/hpblr_tool inspect --input sample.hpblr
```

Dump the first five warning events:

```bash
./build/hpblr_tool dump --input sample.hpblr --severity warning --limit 5
```

Export events from one producer to CSV:

```bash
./build/hpblr_tool export --input sample.hpblr --format csv --producer 2 --output producer2.csv
```

Export errors to JSON:

```bash
./build/hpblr_tool export --input sample.hpblr --format json --severity error --output errors.json
```

Run a benchmark and write a JSON report:

```bash
./build/hpblr_bench --output bench.hpblr --report bench_report.json --producers 8 --events 1000000 --payload-size 128
```

## CLI tools

### `hpblr_record`

Generates synthetic events from several producer threads and writes them through the asynchronous recorder.

Important options:

```text
--output <file>          Output .hpblr file
--producers <n>         Number of producer threads
--events <n>            Total events, 0 means run until Ctrl-C
--payload-size <bytes>  Payload size per event
--queue-capacity <n>    Bounded queue capacity
--flush-bytes <n>       Writer flush threshold
--sleep-us <n>          Optional producer pacing delay
--log-file <file>       Append logs to a text file
```

### `hpblr_tool`

Reads `.hpblr` files and validates CRCs while replaying events.

Supported commands:

```text
inspect --input <file>
dump --input <file> [filters] [--limit <n>]
export --input <file> --format csv|json [--output <file>] [filters] [--limit <n>]
```

Filters:

```text
--producer <id>
--type <id>
--severity trace|debug|info|warning|error|critical
--from-ns <timestamp>
--to-ns <timestamp>
```

### `hpblr_bench`

Measures end-to-end event generation, queueing and binary writing throughput. It reports event rate, file size and MiB/s.

## Binary format

The `.hpblr` format is intentionally simple and documented in [`docs/BINARY_FORMAT.md`](docs/BINARY_FORMAT.md). It uses:

- a versioned file header;
- one fixed-size header per event;
- little-endian integer fields;
- CRC-32 over the file header;
- CRC-32 over each record header;
- CRC-32 over each payload.

## Tests

The test suite covers:

- CRC-32 known vector;
- binary write/read roundtrip;
- payload corruption detection;
- truncated file detection;
- multi-producer asynchronous recording.

Run:

```bash
ctest --test-dir build --output-on-failure
```

## Demo

```bash
./scripts/demo.sh
```

The script builds the project, records a sample file, inspects it, exports CSV and runs a small benchmark.
