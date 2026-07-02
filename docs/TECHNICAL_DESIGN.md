# Technical Design

## Objective

The High-Performance Binary Log Recorder is a C++20 software black box for technical events. It is designed to receive a continuous stream of events from multiple producers, centralize them through a bounded queue, and write them to a binary file with deterministic replay and corruption detection.

## Main components

### Core event model

`hpblr::Event` contains:

- `timestamp_ns`: Unix timestamp in nanoseconds;
- `sequence`: monotonic sequence number assigned by the producer application;
- `producer_id`: source identifier;
- `type`: application-defined event type;
- `severity`: trace/debug/info/warning/error/critical;
- `payload`: opaque bytes.

The core library does not interpret payload contents. This keeps the recorder generic and suitable for telemetry, instrumentation, hardware traces or simulation logs.

### Blocking queue

`hpblr::BlockingQueue<T>` is a bounded thread-safe queue implemented with:

- `std::mutex` to protect the shared queue state;
- `std::condition_variable` to block producers when the queue is full and block the writer when it is empty;
- `close()` semantics to wake all waiting threads during shutdown.

The queue is intentionally simple and predictable. A bounded queue prevents unbounded memory growth when producers are faster than the writer.

### Async recorder

`hpblr::AsyncRecorder` owns:

- one `BinaryLogWriter`;
- one `BlockingQueue<Event>`;
- one background `std::jthread` that drains the queue and writes records.

Producers call `submit(Event)`. The writer thread is the only component that touches the file writer, so the file serialization path does not need a per-record file mutex.

### Binary writer

`hpblr::BinaryLogWriter` writes a versioned `.hpblr` file. It buffers records in memory and flushes when a configurable threshold is reached. This reduces syscall overhead compared with writing every event independently.

### Binary reader

`hpblr::BinaryLogReader` validates the file header and each record during replay. It returns `std::optional<Event>` so callers can iterate until end-of-file without using sentinel objects.

### CLI layer

The apps are intentionally thin:

- `hpblr_record`: synthetic multi-producer recorder;
- `hpblr_tool`: inspect, dump, filter and export;
- `hpblr_bench`: benchmark and JSON report generation.

## Error handling strategy

The project uses controlled exceptions derived from `hpblr::HpblrError` for library-level failures:

- `IoError`: file open/write/read failures;
- `FileFormatError`: malformed or unsupported files;
- `CrcError`: header or payload corruption;
- `QueueClosedError`: reserved for queue misuse scenarios.

The CLI catches `std::exception` at the boundary and exits with code `1` on failure.

## Threading model

```text
Producer 0 ┐
Producer 1 ├── submit(Event) ──> BlockingQueue<Event> ──> Writer thread ──> .hpblr
Producer N ┘
```

The writer thread drains until the queue is closed and empty. Shutdown is deterministic:

1. producers stop generating events;
2. `AsyncRecorder::stop()` closes the queue;
3. the writer drains remaining events;
4. the writer flushes the file;
5. `stop()` joins the writer thread.

## Performance considerations

The design is optimized for clarity and real throughput:

- one file writer thread avoids write interleaving;
- buffered writes reduce syscall frequency;
- payloads are moved into the queue instead of copied by API design;
- the queue capacity is configurable;
- the benchmark measures end-to-end throughput, not just synthetic loop speed.

For even higher throughput, future work could add sharded queues, memory-mapped output, batch pop operations, compression blocks or zero-copy payload ownership.

## Robustness considerations

Every file header and record is validated on read. Payload CRCs catch data corruption, while header CRCs catch damaged metadata such as sequence, timestamp, producer id or payload size.

The reader rejects:

- invalid magic values;
- unsupported versions;
- unsupported header sizes;
- invalid endian markers;
- payload sizes above the configured safety limit;
- truncated record headers;
- truncated payloads;
- CRC mismatches.
