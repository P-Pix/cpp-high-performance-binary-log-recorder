#pragma once

#include <stdexcept>
#include <string>

namespace hpblr {

class HpblrError : public std::runtime_error {
public:
    explicit HpblrError(const std::string& message) : std::runtime_error(message) {}
};

class IoError final : public HpblrError {
public:
    explicit IoError(const std::string& message) : HpblrError(message) {}
};

class FileFormatError final : public HpblrError {
public:
    explicit FileFormatError(const std::string& message) : HpblrError(message) {}
};

class CrcError final : public HpblrError {
public:
    explicit CrcError(const std::string& message) : HpblrError(message) {}
};

class QueueClosedError final : public HpblrError {
public:
    explicit QueueClosedError(const std::string& message) : HpblrError(message) {}
};

} // namespace hpblr
