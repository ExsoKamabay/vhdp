#include "vtest.hpp"

#include "vhdp/vhdp.hpp"

#include <string>

VTEST(cpp_wrapper) {
    auto ctx = vhdp::Context::create();
    REQUIRE(ctx.ok());

    auto caps = ctx.value().capabilities_json();
    REQUIRE(caps.ok());
    CHECK_NE(caps.value().find("\"abi_version\""), std::string::npos);
    CHECK_NE(caps.value().find("compatibility isolation"), std::string::npos);

    auto doctor = ctx.value().doctor_json(VHDP_DOCTOR_NO_ACTIVE_PROBES);
    REQUIRE(doctor.ok());
    CHECK_NE(doctor.value().find("\"checks\""), std::string::npos);

    // Config builder + validation error path through the RAII wrapper.
    auto cfg = vhdp::Config::create(ctx.value());
    REQUIRE(cfg.ok());
    CHECK(cfg.value().rootfs("/no/such/vhdp/dir").ok());
    CHECK(cfg.value().engine(vhdp::Engine::Rootless).ok());
    auto session = vhdp::Session::create(ctx.value(), cfg.value());
    CHECK(!session.ok());
    CHECK_EQ(session.status().code(), static_cast<vhdp_status_t>(VHDP_E_ROOTFS_INVALID));
    CHECK(!session.status().message().empty());

    // Status helpers.
    vhdp::Status ok_status;
    CHECK(ok_status.ok());
    vhdp::Status err = vhdp::Status::from_c(VHDP_E_TIMEOUT);
    CHECK(!err.ok());
    CHECK_EQ(std::string(err.name()), "VHDP_E_TIMEOUT");

    // Move semantics: moved-from config/session must be safe to destroy.
    auto cfg2 = vhdp::Config::create(ctx.value());
    REQUIRE(cfg2.ok());
    vhdp::Config moved = std::move(cfg2.value());
    CHECK(moved.rootfs("/tmp").ok());
}
