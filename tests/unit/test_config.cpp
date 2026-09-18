#include "vtest.hpp"

#include "core/config.hpp"

#include <algorithm>

using namespace vhdp::core;
using vhdp::Code;

namespace {
auto no_env = [](const char*) -> std::optional<std::string> { return std::nullopt; };

bool has_env(const std::vector<std::string>& env, const std::string& kv) {
    return std::find(env.begin(), env.end(), kv) != env.end();
}
} // namespace

VTEST(config_validate) {
    SessionConfig c;
    // Missing rootfs.
    CHECK_EQ(validate_config(c, no_env).status().code(), Code::rootfs_invalid);

    c.rootfs = "/definitely/does/not/exist/vhdp";
    CHECK_EQ(validate_config(c, no_env).status().code(), Code::rootfs_invalid);

    // '/' as rootfs is refused.
    c.rootfs = "/";
    CHECK_EQ(validate_config(c, no_env).status().code(), Code::rootfs_invalid);

    // Guest path checks (pure, no filesystem).
    CHECK(check_guest_path("/ok", "cwd").is_ok());
    CHECK(!check_guest_path("rel", "cwd").is_ok());
    CHECK(!check_guest_path("/a/../b", "cwd").is_ok());
    CHECK(!check_guest_path(std::string_view("/a\0b", 4), "cwd").is_ok());

    CHECK(valid_env_entry("A=b"));
    CHECK(valid_env_entry("A="));
    CHECK(!valid_env_entry("=b"));
    CHECK(!valid_env_entry("noeq"));
}

VTEST(config_env) {
    SessionConfig c;
    c.rootfs = "/tmp"; // an existing directory; not '/'
    c.uid = 0;
    c.env.push_back("PATH=/custom");
    c.env.push_back("MY=1");
    auto r = validate_config(c, [](const char* n) -> std::optional<std::string> {
        if (std::string(n) == "TERM") {
            return std::string("xterm");
        }
        return std::nullopt;
    });
    REQUIRE(r.is_ok());
    const auto& env = r.value()->final_env;
    CHECK(has_env(env, "PATH=/custom")); // override wins over the default PATH
    CHECK(has_env(env, "HOME=/root"));   // uid 0 -> /root
    CHECK(has_env(env, "USER=root"));
    CHECK(has_env(env, "TERM=xterm")); // allowlisted host var
    CHECK(has_env(env, "MY=1"));
    // Only one PATH entry.
    CHECK_EQ(std::count_if(env.begin(), env.end(),
                           [](const std::string& e) { return e.rfind("PATH=", 0) == 0; }),
             1);

    SessionConfig cleared = c;
    cleared.clear_env = true;
    cleared.env = {"ONLY=this"};
    auto r2 = validate_config(cleared, no_env);
    REQUIRE(r2.is_ok());
    CHECK_EQ(r2.value()->final_env.size(), 1u);
    CHECK(has_env(r2.value()->final_env, "ONLY=this"));
}
