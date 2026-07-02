#pragma once

#include <cstdint>
#include <string>

namespace hpblr {

[[nodiscard]] std::uint64_t now_unix_ns();
[[nodiscard]] std::string format_unix_ns_utc(std::uint64_t timestamp_ns);

} // namespace hpblr
