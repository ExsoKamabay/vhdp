#include "harness.hpp"
#include "vtest.hpp"

#include "vhdp/vhdp.hpp"

#include <csignal>
#include <string>

using namespace it;

VTEST(cli_timeout) {
    // --timeout kills the guest tree; shell status 124.
    RunResult r = run_phdp(
        {"run", "--timeout", "0.4", fixture_rootfs(), "--", "/bin/ready-sleep", "30"}, "", 8000);
    CHECK(!r.timed_out);
    CHECK_EQ(r.shell_status, 124);
}

VTEST(cli_signal_forward) {
    // Deterministic signal delivery through the library: start a sleeper, wait for
    // it to be running, send SIGTERM, and confirm it dies of the signal (143).
    auto ctx = vhdp::Context::create();
    REQUIRE(ctx.ok());
    auto cfg = vhdp::Config::create(ctx.value());
    REQUIRE(cfg.ok());
    REQUIRE(cfg.value().rootfs(fixture_rootfs()).ok());
    REQUIRE(cfg.value().argv({"/bin/ready-sleep", "30"}).ok());
    auto session = vhdp::Session::create(ctx.value(), cfg.value());
    REQUIRE(session.ok());
    REQUIRE(session.value().start().ok());

    // Poll until running, then signal.
    for (int i = 0; i < 200; ++i) {
        if (session.value().state() == VHDP_STATE_RUNNING) {
            break;
        }
        struct timespec ts{0, 5 * 1000 * 1000};
        nanosleep(&ts, nullptr);
    }
    // Give the guest a moment to install nothing (it uses default SIGTERM).
    struct timespec ts{0, 100 * 1000 * 1000};
    nanosleep(&ts, nullptr);
    CHECK(session.value().signal(SIGTERM).ok());

    auto info = session.value().wait(5000);
    REQUIRE(info.ok());
    CHECK_EQ(info.value().reason, static_cast<vhdp_exit_reason_t>(VHDP_EXIT_SIGNALED));
    CHECK_EQ(info.value().term_signal, SIGTERM);
    CHECK_EQ(info.value().shell_status, 128 + SIGTERM);
}
