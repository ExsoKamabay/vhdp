// Minimal C++ embedding example using the header-only RAII wrapper.
//
//   embed_run_cpp ROOTFS COMMAND [ARG...]
#include <vhdp/vhdp.hpp>

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s ROOTFS COMMAND [ARG...]\n", argv[0]);
        return 2;
    }
    auto ctx = vhdp::Context::create(
        [](const vhdp::Event& e) {
            if (e.severity >= VHDP_SEVERITY_WARNING) {
                std::fprintf(stderr, "embed_run_cpp: event %.*s: %.*s\n",
                             static_cast<int>(e.name.size()), e.name.data(),
                             static_cast<int>(e.message.size()), e.message.data());
            }
        },
        VHDP_SEVERITY_WARNING);
    if (!ctx) {
        std::fprintf(stderr, "embed_run_cpp: %s\n", ctx.status().message().c_str());
        return 125;
    }
    auto cfg = vhdp::Config::create(ctx.value());
    if (!cfg) {
        return 125;
    }
    std::vector<std::string> command(argv + 2, argv + argc);
    for (vhdp::Status s : {cfg.value().rootfs(argv[1]), cfg.value().argv(command)}) {
        if (!s) {
            std::fprintf(stderr, "embed_run_cpp: config: %s %s\n", s.name(), s.message().c_str());
            return 125;
        }
    }
    auto session = vhdp::Session::create(ctx.value(), cfg.value());
    if (!session) {
        std::fprintf(stderr, "embed_run_cpp: session: %s\n", session.status().message().c_str());
        return 125;
    }
    if (vhdp::Status s = session.value().start(); !s) {
        std::fprintf(stderr, "embed_run_cpp: start: %s\n", s.message().c_str());
        auto info = session.value().wait(0);
        return info && info.value().shell_status != 0 ? info.value().shell_status : 125;
    }
    auto info = session.value().wait();
    return info ? info.value().shell_status : 125;
}
