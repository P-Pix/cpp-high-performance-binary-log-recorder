#include "hpblr/time_utils.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace hpblr {

std::uint64_t now_unix_ns() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::string format_unix_ns_utc(std::uint64_t timestamp_ns) {
    const auto seconds = static_cast<std::time_t>(timestamp_ns / 1'000'000'000ULL);
    const auto nanos = timestamp_ns % 1'000'000'000ULL;

    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &seconds);
#else
    gmtime_r(&seconds, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(9) << std::setfill('0') << nanos << 'Z';
    return oss.str();
}

} // namespace hpblr
