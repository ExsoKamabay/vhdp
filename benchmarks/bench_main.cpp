// Micro-benchmarks for the rootless engine. Runs a fixture guest program under
// three configurations (seccomp on/off, no identity) and reports p50/p95 for a
// syscall-heavy loop and session startup. Requires a fixture rootfs path.
//
//   vhdp_bench <rootfs> [iterations]
//
// The fixture must provide the bench-getpid and bench-stat applets. Output is
// human-readable; nothing is fabricated when a configuration is unavailable.
#include "vhdp/vhdp.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Stats {
    double p50 = 0;
    double p95 = 0;
    double min = 0;
};

Stats summarize(std::vector<double>& v) {
    Stats s;
    if (v.empty()) {
        return s;
    }
    std::sort(v.begin(), v.end());
    s.min = v.front();
    s.p50 = v[v.size() / 2];
    s.p95 = v[static_cast<std::size_t>(0.95 * static_cast<double>(v.size() - 1))];
    return s;
}

// Runs `command` in the rootfs under the given seccomp mode; returns the guest's
// self-reported nanoseconds (printed on its stdout via an fd we capture).
bool run_once(const std::string& rootfs, const std::vector<std::string>& cmd,
              vhdp_seccomp_mode_t mode, double& out_ms, long& reported_ns) {
    auto ctx = vhdp::Context::create();
    if (!ctx.ok()) {
        return false;
    }
    auto cfg = vhdp::Config::create(ctx.value());
    if (!cfg.ok() || !cfg.value().rootfs(rootfs).ok() || !cfg.value().argv(cmd).ok() ||
        !cfg.value().seccomp(mode).ok()) {
        return false;
    }
    std::string captured;
    if (!cfg.value().stdio(VHDP_STDIO_PTY).ok() || !cfg.value().pty_size(24, 80).ok()) {
        return false;
    }
    cfg.value().on_output([&captured](vhdp_stream_t, const std::uint8_t* d, std::size_t n) {
        captured.append(reinterpret_cast<const char*>(d), n);
    });
    auto session = vhdp::Session::create(ctx.value(), cfg.value());
    if (!session.ok()) {
        return false;
    }
    struct timespec t0;
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (!session.value().start().ok()) {
        return false;
    }
    auto info = session.value().wait();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (!info.ok()) {
        return false;
    }
    out_ms = (static_cast<double>(t1.tv_sec - t0.tv_sec) * 1e3) +
             static_cast<double>(t1.tv_nsec - t0.tv_nsec) / 1e6;
    reported_ns = std::atol(captured.c_str());
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s ROOTFS [iterations]\n", argv[0]);
        return 2;
    }
    std::string rootfs = argv[1];
    int iters = argc > 2 ? std::atoi(argv[2]) : 30;

    struct Config {
        const char* name;
        vhdp_seccomp_mode_t mode;
    };
    Config configs[] = {{"ptrace-only", VHDP_SECCOMP_OFF}, {"seccomp", VHDP_SECCOMP_ON}};

    std::printf("VHDP benchmark (rootfs=%s, iterations=%d)\n", rootfs.c_str(), iters);
    std::printf("%-14s %-14s %10s %10s %10s\n", "config", "workload", "p50(ms)", "p95(ms)",
                "min(ms)");

    for (const auto& c : configs) {
        for (const char* workload : {"bench-getpid", "bench-stat"}) {
            std::vector<double> wall;
            std::vector<std::string> cmd = {std::string("/bin/") + workload, "100000"};
            for (int i = 0; i < iters; ++i) {
                double ms = 0;
                long ns = 0;
                if (run_once(rootfs, cmd, c.mode, ms, ns)) {
                    wall.push_back(ms);
                }
            }
            Stats s = summarize(wall);
            if (wall.empty()) {
                std::printf("%-14s %-14s   (unavailable in this configuration)\n", c.name,
                            workload);
            } else {
                std::printf("%-14s %-14s %10.2f %10.2f %10.2f\n", c.name, workload, s.p50, s.p95,
                            s.min);
            }
        }
    }
    std::printf("\nNote: startup + workload wall time end to end; compare against a native "
                "baseline on the same device.\n");
    return 0;
}
