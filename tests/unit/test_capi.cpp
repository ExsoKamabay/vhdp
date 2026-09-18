#include "vtest.hpp"

#include "vhdp/vhdp.h"

#include <cstring>
#include <string>

// White-box-free: exercises the C ABI exactly as an external consumer would.
VTEST(capi_lifecycle) {
    CHECK_EQ(vhdp_abi_version(), VHDP_ABI_VERSION);
    CHECK(std::string(vhdp_version_string()).size() >= 5);
    CHECK_EQ(std::string(vhdp_status_name(VHDP_E_ROOTFS_INVALID)), "VHDP_E_ROOTFS_INVALID");
    CHECK_EQ(std::string(vhdp_status_name(4242)), "VHDP_E_UNKNOWN");

    vhdp_context_options opts{};
    opts.struct_size = sizeof(opts);
    opts.abi_version = VHDP_ABI_VERSION;
    vhdp_context* ctx = nullptr;
    REQUIRE_EQ(vhdp_context_create(&opts, &ctx), VHDP_OK);
    REQUIRE(ctx != nullptr);

    // capabilities JSON: too-small buffer returns the needed size.
    size_t needed = 0;
    CHECK_EQ(vhdp_capabilities_json(ctx, nullptr, 0, &needed), VHDP_E_BUFFER_TOO_SMALL);
    CHECK(needed > 2);
    std::string buf(needed, '\0');
    REQUIRE_EQ(vhdp_capabilities_json(ctx, buf.data(), buf.size(), &needed), VHDP_OK);
    CHECK_EQ(buf[needed - 1], '\0');

    // Config validation error surfaces through last_error_message.
    vhdp_config* cfg = nullptr;
    REQUIRE_EQ(vhdp_config_create(ctx, &cfg), VHDP_OK);
    CHECK_EQ(vhdp_config_set_rootfs(cfg, "/nonexistent/vhdp/path"), VHDP_OK);
    vhdp_session* s = nullptr;
    vhdp_status_t st = vhdp_session_create(ctx, cfg, &s);
    CHECK_EQ(st, VHDP_E_ROOTFS_INVALID);
    char msg[256];
    size_t mneeded = 0;
    CHECK_EQ(vhdp_last_error_message(msg, sizeof(msg), &mneeded), VHDP_OK);
    CHECK(std::strlen(msg) > 0);

    // Argument validation.
    CHECK_EQ(vhdp_config_set_cwd(cfg, "relative"), VHDP_E_INVALID_ARGUMENT);
    CHECK_EQ(vhdp_config_add_env(cfg, "noequals"), VHDP_E_INVALID_ARGUMENT);
    CHECK_EQ(vhdp_config_add_bind(cfg, "/h", "relative", VHDP_BIND_READ_ONLY), VHDP_E_BIND_INVALID);
    CHECK_EQ(vhdp_config_set_uid(cfg, 0xffffffffu), VHDP_E_INVALID_ARGUMENT);
    CHECK_EQ(vhdp_config_set_engine(cfg, 999), VHDP_E_INVALID_ARGUMENT);

    vhdp_config_destroy(cfg);
    // Destroy with no live sessions works; NULL is OK.
    CHECK_EQ(vhdp_context_destroy(ctx), VHDP_OK);
    CHECK_EQ(vhdp_context_destroy(nullptr), VHDP_OK);
    CHECK_EQ(vhdp_session_destroy(nullptr), VHDP_OK);
}

VTEST(capi_abi_guard) {
    // Wrong struct_size / abi_version is rejected as an ABI mismatch.
    vhdp_context_options bad{};
    bad.struct_size = 4; // too small
    bad.abi_version = VHDP_ABI_VERSION;
    vhdp_context* ctx = nullptr;
    CHECK_EQ(vhdp_context_create(&bad, &ctx), VHDP_E_ABI_MISMATCH);

    vhdp_context_options bad2{};
    bad2.struct_size = sizeof(bad2);
    bad2.abi_version = 999;
    CHECK_EQ(vhdp_context_create(&bad2, &ctx), VHDP_E_ABI_MISMATCH);

    // Non-zero reserved fields are rejected.
    vhdp_context_options bad3{};
    bad3.struct_size = sizeof(bad3);
    bad3.abi_version = VHDP_ABI_VERSION;
    bad3.reserved[0] = 1;
    CHECK_EQ(vhdp_context_create(&bad3, &ctx), VHDP_E_INVALID_ARGUMENT);

    // NULL out pointer.
    CHECK_EQ(vhdp_context_create(nullptr, nullptr), VHDP_E_INVALID_ARGUMENT);
}
