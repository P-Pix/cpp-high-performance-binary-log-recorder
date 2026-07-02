#include "test_framework.hpp"

#include "hpblr/crc32.hpp"

TEST_CASE("crc32_known_vector") {
    REQUIRE_EQ(hpblr::crc32("123456789"), 0xCBF43926U);
}

TEST_CASE("crc32_empty_buffer") {
    REQUIRE_EQ(hpblr::crc32(""), 0U);
}
