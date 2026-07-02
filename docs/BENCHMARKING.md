# Benchmarking Guide

## Running a benchmark

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/hpblr_bench --output bench.hpblr --report bench_report.json --producers 8 --events 1000000 --payload-size 128
```

The benchmark measures the complete path:

1. synthetic event generation;
2. queue submission from multiple producers;
3. background writer consumption;
4. binary serialization;
5. file writes and final flush.

It intentionally does not measure a fake no-op writer, because the goal is to represent a practical logging pipeline.

## Important parameters

- `--producers`: number of producer threads;
- `--events`: total event count;
- `--payload-size`: payload bytes per event;
- `--queue-capacity`: bounded queue depth;
- `--flush-bytes`: writer buffer flush threshold.

## Interpreting results

The tool prints:

- `events`: number of events successfully written;
- `duration_seconds`: wall-clock duration;
- `events_per_second`: event throughput;
- `file_size_bytes`: generated binary file size;
- `mib_per_second`: file throughput.

Throughput depends on CPU, filesystem, storage device, build type, payload size and producer count. Always compare results on the same machine and with the same build configuration.

## Suggested experiments

```bash
# Small payloads, many events
./build/hpblr_bench --events 2000000 --payload-size 16 --producers 4

# Larger payloads
./build/hpblr_bench --events 500000 --payload-size 1024 --producers 4

# More producers
./build/hpblr_bench --events 1000000 --payload-size 128 --producers 12

# Smaller queue to observe backpressure
./build/hpblr_bench --events 500000 --payload-size 128 --queue-capacity 64
```

## Possible future optimizations

- batch popping from the queue;
- one queue per producer with a merge writer;
- memory-mapped output segments;
- direct I/O experiments;
- compression blocks;
- payload schema-specific encoders.
