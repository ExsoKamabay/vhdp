#include "harness.hpp"
#include "vtest.hpp"

#include "vhdp/vhdp.hpp"

#include <spawn.h>
#include <sys/wait.h>

#include <mutex>
#include <string>
#include <vector>

extern char** environ;

using namespace it;

namespace {

int run_embed(const char* bin, const std::vector<std::string>& extra) {
    std::vector<std::string> argv_s = {bin, fixture_rootfs()};
    for (const auto& e : extra) {
        argv_s.push_back(e);
    }
    std::vector<char*> argv;
    for (auto& a : argv_s) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    pid_t pid = -1;
    if (posix_spawn(&pid, bin, nullptr, nullptr, argv.data(), environ) != 0) {
        return -1;
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace

VTEST(ex_c_embed) {
    // The C example runs the same session as the CLI and propagates exit status.
    CHECK_EQ(run_embed(EMBED_C_BIN, {"/bin/exit", "37"}), 37);
    CHECK_EQ(run_embed(EMBED_C_BIN, {"/bin/echo", "x"}), 0);
}

VTEST(ex_cpp_embed) {
    CHECK_EQ(run_embed(EMBED_CPP_BIN, {"/bin/exit", "5"}), 5);
    CHECK_EQ(run_embed(EMBED_CPP_BIN, {"/bin/echo", "x"}), 0);
}

VTEST(lib_pty_resize) {
    // Library PTY mode: the guest waits for SIGWINCH, we resize, it observes the
    // new window size. Output is captured through the output callback.
    auto ctx = vhdp::Context::create();
    REQUIRE(ctx.ok());
    auto cfg = vhdp::Config::create(ctx.value());
    REQUIRE(cfg.ok());
    // The output callback runs on the library's pump thread, so access to the
    // captured buffer from this (main) thread must be synchronised.
    std::mutex cap_mu;
    std::string captured;
    auto snapshot = [&] {
        std::lock_guard<std::mutex> lock(cap_mu);
        return captured;
    };
    REQUIRE(cfg.value().rootfs(fixture_rootfs()).ok());
    REQUIRE(cfg.value().argv({"/bin/wait-winch"}).ok());
    REQUIRE(cfg.value().stdio(VHDP_STDIO_PTY).ok());
    REQUIRE(cfg.value().pty_size(24, 80).ok());
    REQUIRE(cfg.value()
                .on_output([&](vhdp_stream_t, const std::uint8_t* d, std::size_t n) {
                    std::lock_guard<std::mutex> lock(cap_mu);
                    captured.append(reinterpret_cast<const char*>(d), n);
                })
                .ok());
    auto session = vhdp::Session::create(ctx.value(), cfg.value());
    REQUIRE(session.ok());
    REQUIRE(session.value().start().ok());
    // Wait until the guest printed its initial size, then resize.
    for (int i = 0; i < 200 && snapshot().find("size ") == std::string::npos; ++i) {
        struct timespec ts{0, 10 * 1000 * 1000};
        nanosleep(&ts, nullptr);
    }
    CHECK(session.value().resize(40, 132).ok());
    auto info = session.value().wait(5000);
    REQUIRE(info.ok());
    CHECK_EQ(info.value().shell_status, 0);
    std::string final = snapshot();
    CHECK_NE(final.find("size 24 80"), std::string::npos);
    CHECK_NE(final.find("resized 40 132"), std::string::npos);
}
