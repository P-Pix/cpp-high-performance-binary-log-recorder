# Robustness Notes

## Failure cases covered by tests

The test suite currently verifies:

- CRC-32 implementation against a known vector;
- write/read roundtrip for many events;
- detection of payload corruption;
- detection of truncated or malformed files;
- multi-producer asynchronous recording.

## Expected reader behavior

The reader fails fast on malformed data. This is intentional because replaying a partially corrupted binary stream without clear recovery markers could hide data integrity issues.

Errors are represented as exceptions:

- `FileFormatError` for structural problems;
- `CrcError` for checksum mismatches;
- `IoError` for filesystem failures.

## Backpressure

The queue is bounded. If producers are faster than the writer, `submit()` blocks until the writer drains space. This is appropriate for a black-box recorder when losing data is worse than applying backpressure.

A future non-blocking mode could expose `try_submit()` and count dropped events for ultra-low-latency use cases.

## Shutdown

Shutdown is designed to preserve all submitted events:

1. stop producers;
2. close the queue;
3. drain all queued events;
4. flush output;
5. join the writer thread.

`hpblr_record` handles `SIGINT` and `SIGTERM` by asking producers to stop, then letting the recorder flush what was already submitted.
