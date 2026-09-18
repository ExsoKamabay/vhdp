#include "vtest.hpp"

#include "common/json.hpp"

using namespace vhdp;

VTEST(json_roundtrip) {
    JsonWriter w;
    w.begin_object();
    w.key("name").value("vhdp");
    w.key("count").value(static_cast<std::int64_t>(-42));
    w.key("big").value(static_cast<std::uint64_t>(18446744073709551615ull));
    w.key("flag").value(true);
    w.key("nothing").null();
    w.key("list").begin_array().value(1).value(2).value("three").end_array();
    w.key("nested").begin_object().key("k").value("v").end_object();
    w.end_object();
    const std::string& s = w.str();
    CHECK(json_validate(s));
    CHECK_NE(s.find("\"big\":18446744073709551615"), std::string::npos);
    CHECK_NE(s.find("\"count\":-42"), std::string::npos);
    CHECK_NE(s.find("[1,2,\"three\"]"), std::string::npos);
}

VTEST(json_escape) {
    CHECK_EQ(json_quote("a\"b\\c"), "\"a\\\"b\\\\c\"");
    CHECK_EQ(json_quote(std::string_view("tab\tnl\n")), "\"tab\\tnl\\n\"");
    // Invalid UTF-8 byte becomes U+FFFD, keeping the output valid.
    std::string bad = "x\xff"
                      "y";
    std::string q = json_quote(bad);
    CHECK(json_validate(q));
    CHECK_NE(q.find("\\ufffd"), std::string::npos);
    // Valid multibyte UTF-8 is preserved verbatim.
    CHECK_EQ(json_quote("é"), std::string("\"é\""));
}

VTEST(json_validate) {
    CHECK(json_validate("{}"));
    CHECK(json_validate("[1,2,3]"));
    CHECK(json_validate("  \"hi\"  "));
    CHECK(json_validate("{\"a\":{\"b\":[true,false,null,1.5e3]}}"));
    CHECK(!json_validate(""));
    CHECK(!json_validate("{"));
    CHECK(!json_validate("{\"a\":}"));
    CHECK(!json_validate("[1,]"));
    CHECK(!json_validate("{\"a\":1} trailing"));
    CHECK(!json_validate("'single'"));
    CHECK(!json_validate("\"\x01\"")); // raw control char
    CHECK(!json_validate("01"));       // leading zero
}
