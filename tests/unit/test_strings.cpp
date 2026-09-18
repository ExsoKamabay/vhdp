#include "vtest.hpp"

#include "common/strings.hpp"

using namespace vhdp;

VTEST(strings_parse) {
    CHECK_EQ(parse_u64("0").value(), 0u);
    CHECK_EQ(parse_u64("18446744073709551615").value(), 18446744073709551615ull);
    CHECK(!parse_u64("18446744073709551616").has_value()); // overflow
    CHECK(!parse_u64("").has_value());
    CHECK(!parse_u64("12a").has_value());
    CHECK(!parse_u64("-1").has_value());

    CHECK_EQ(parse_u32("4294967294").value(), 4294967294u);
    CHECK(!parse_u32("4294967296").has_value());

    CHECK_EQ(parse_i32("-2147483648").value(), -2147483648);
    CHECK_EQ(parse_i32("2147483647").value(), 2147483647);
    CHECK(!parse_i32("2147483648").has_value());

    auto parts = split("a:b::c", ':');
    CHECK_EQ(parts.size(), 4u);
    CHECK_EQ(std::string(parts[2]), "");
    CHECK(contains_nul(std::string_view("a\0b", 3)));
    CHECK(!contains_nul("abc"));
}
