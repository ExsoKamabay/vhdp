#include "vtest.hpp"

#include "vfs/guest_path.hpp"

using namespace vhdp::vfs;

VTEST(guest_path_components) {
    CHECK(is_absolute("/a"));
    CHECK(!is_absolute("a"));
    CHECK(!is_absolute(""));

    auto c = components("/a//b/./c/");
    REQUIRE_EQ(c.size(), 3u);
    CHECK_EQ(std::string(c[0]), "a");
    CHECK_EQ(std::string(c[1]), "b");
    CHECK_EQ(std::string(c[2]), "c");

    CHECK(has_dotdot("/a/../b"));
    CHECK(!has_dotdot("/a/b"));
    CHECK(!has_dotdot("/a/..b"));

    CHECK(is_below("/a/b", "/a"));
    CHECK(is_below("/a", "/a"));
    CHECK(!is_below("/ab", "/a"));
    CHECK(is_below("/anything", "/"));
    CHECK_EQ(std::string(suffix_after("/a/b", "/a")), "/b");
    CHECK_EQ(std::string(suffix_after("/a", "/a")), "");
}

VTEST(guest_path_normalize) {
    CHECK_EQ(normalize_lexical("/a/b/../c"), "/a/c");
    CHECK_EQ(normalize_lexical("/a/../../b"), "/b"); // clamps at root
    CHECK_EQ(normalize_lexical("/"), "/");
    CHECK_EQ(normalize_lexical("/a/./b//c"), "/a/b/c");
    CHECK_EQ(normalize_lexical("/.."), "/");
    CHECK_EQ(join("/base", "rel"), "/base/rel");
    CHECK_EQ(join("/base/", "rel"), "/base/rel");
    CHECK_EQ(join("/base", "/abs"), "/abs");
}
