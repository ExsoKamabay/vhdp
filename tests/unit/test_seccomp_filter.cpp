#include "vtest.hpp"

#include "engines/rootless/seccomp_filter.hpp"

#include <linux/audit.h>
#include <linux/seccomp.h>
#include <sys/socket.h>

#include <cerrno>

using namespace vhdp::rootless;

namespace {
#if defined(__x86_64__)
constexpr std::uint32_t kArch = AUDIT_ARCH_X86_64;
#else
constexpr std::uint32_t kArch = AUDIT_ARCH_AARCH64;
#endif

FilterInput in(int nr, std::uint32_t arch = kArch) {
    FilterInput i;
    i.nr = nr;
    i.arch = arch;
    return i;
}
} // namespace

VTEST(seccomp_filter) {
    FilterPolicy p;
    p.audit_arch = kArch;
    p.allow = {10, 11, 12, 200}; // contiguous range 10..12 plus 200
    std::uint32_t trace = SECCOMP_RET_TRACE | kSeccompTraceMarker;

    auto prog = build_seccomp_filter(p);

    // Allowed numbers -> ALLOW.
    CHECK_EQ(evaluate_seccomp_filter(prog, in(10)), SECCOMP_RET_ALLOW);
    CHECK_EQ(evaluate_seccomp_filter(prog, in(11)), SECCOMP_RET_ALLOW);
    CHECK_EQ(evaluate_seccomp_filter(prog, in(200)), SECCOMP_RET_ALLOW);
    // Not allowed -> TRACE (decided by the supervisor).
    CHECK_EQ(evaluate_seccomp_filter(prog, in(13)), trace);
    CHECK_EQ(evaluate_seccomp_filter(prog, in(9)), trace);
    // Foreign architecture -> TRACE.
    CHECK_EQ(evaluate_seccomp_filter(prog, in(10, 0xdead)), trace);

    // Network-none: socket(AF_UNIX) allowed, socket(AF_INET) -> EACCES.
    FilterPolicy np;
    np.audit_arch = kArch;
    np.network_none = true;
    np.nr_socket = 41;
    auto nprog = build_seccomp_filter(np);
    FilterInput unix_sock = in(41);
    unix_sock.args[0] = AF_UNIX;
    FilterInput inet_sock = in(41);
    inet_sock.args[0] = AF_INET;
    CHECK_EQ(evaluate_seccomp_filter(nprog, unix_sock), SECCOMP_RET_ALLOW);
    CHECK_EQ(evaluate_seccomp_filter(nprog, inet_sock),
             SECCOMP_RET_ERRNO | static_cast<std::uint32_t>(EACCES));

    // clone with CLONE_UNTRACED must be traced (so the flag can be stripped).
    FilterPolicy cp;
    cp.audit_arch = kArch;
    cp.nr_clone = 56;
    cp.allow = {56};
    auto cprog = build_seccomp_filter(cp);
    FilterInput plain_clone = in(56);
    plain_clone.args[0] = 0;
    FilterInput untraced = in(56);
    untraced.args[0] = 0x00800000; // CLONE_UNTRACED
    CHECK_EQ(evaluate_seccomp_filter(cprog, plain_clone), SECCOMP_RET_ALLOW);
    CHECK_EQ(evaluate_seccomp_filter(cprog, untraced), trace);
}
