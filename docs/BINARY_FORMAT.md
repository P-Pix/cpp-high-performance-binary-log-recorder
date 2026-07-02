# HPBLR Binary Format

The file extension used by the project is `.hpblr`.

All integer fields are encoded in little-endian order. The current format version is `1`.

## File header

Size: 36 bytes.

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 8 | magic | ASCII `HPBLOG1\0` |
| 8 | 2 | version | Current value: `1` |
| 10 | 2 | header_size | Current value: `36` |
| 12 | 2 | endian_marker | Current value: `0x0102` |
| 14 | 2 | flags | Reserved, current value: `0` |
| 16 | 8 | created_unix_ns | Creation timestamp in Unix nanoseconds |
| 24 | 8 | reserved | Reserved, current value: `0` |
| 32 | 4 | header_crc32 | CRC-32 over bytes `[0, 32)` |

The reader rejects files with invalid magic, unsupported version, unsupported header size, unsupported endian marker or header CRC mismatch.

## Record header

Size: 48 bytes.

| Offset | Size | Field | Description |
|---:|---:|---|---|
| 0 | 4 | magic | Little-endian value `0x524C4248` |
| 4 | 2 | header_size | Current value: `48` |
| 6 | 2 | version | Current value: `1` |
| 8 | 8 | timestamp_ns | Event timestamp in Unix nanoseconds |
| 16 | 8 | sequence | Application-assigned monotonic sequence |
| 24 | 4 | producer_id | Event producer id |
| 28 | 2 | type | Application-defined event type |
| 30 | 2 | severity | 0 trace, 1 debug, 2 info, 3 warning, 4 error, 5 critical |
| 32 | 4 | payload_size | Payload size in bytes |
| 36 | 4 | header_crc32 | CRC-32 over the 48-byte header with this field set to zero |
| 40 | 4 | payload_crc32 | CRC-32 over the payload bytes |
| 44 | 4 | flags | Reserved, current value: `0` |

The payload immediately follows the record header.

## CRC choices

The project uses the standard CRC-32 polynomial `0xEDB88320`. The classic test vector `123456789` must produce `0xCBF43926`.

CRC-32 is not a cryptographic signature. It is used here for accidental corruption detection, not tamper resistance.

## Streaming and replay

Records are appended one after another. There is no global event count in the file header because the writer can stream indefinitely and still produce a valid file after clean shutdown.

The reader stops at end-of-file only when it is positioned exactly at a record boundary. A partial header or partial payload is treated as corruption.

## Safety limit

The reader rejects payload sizes above 16 MiB by default. This protects replay tools from trying to allocate huge buffers when reading a corrupted size field.
