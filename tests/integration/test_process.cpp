#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(proc_fanout) {
    // fork/vfork/clone + threads: the guest forks many children and joins threads;
    // every descendant is traced and reaped without loss.
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/fanout", "64"}, "", 15000);
    CHECK(!r.timed_out);
    CHECK_EQ(r.shell_status, 0);
    CHECK_NE(r.out.find("fanout ok"), std::string::npos);
}

VTEST(syscall_unsupported) {
    // mount() is always refused with EPERM plus a diagnostic; the guest keeps
    // running and reports the errno itself.
    RunResult m = run_phdp({"run", "--json", fixture_rootfs(), "--", "/bin/try", "mount"});
    CHECK_EQ(m.shell_status, 0);
    CHECK_NE(m.out.find("EPERM"), std::string::npos);
    CHECK_NE(m.err.find("denied"), std::string::npos);
    // chroot is refused too.
    RunResult c = run_phdp({fixture_rootfs(), "--", "/bin/try", "chroot", "/"});
    CHECK_NE(c.out.find("EPERM"), std::string::npos);
    // unshare is refused.
    RunResult u = run_phdp({fixture_rootfs(), "--", "/bin/try", "unshare"});
    CHECK_NE(u.out.find("EPERM"), std::string::npos);
}

VTEST(syscall_ptrace_only) {
    // The same workload must pass with seccomp disabled (pure PTRACE_SYSCALL path).
    RunResult r = run_phdp({"run", "--seccomp", "off", fixture_rootfs(), "--", "/bin/fanout", "32"},
                           "", 15000);
    CHECK(!r.timed_out);
    CHECK_EQ(r.shell_status, 0);
    // And with seccomp forced on.
    RunResult on = run_phdp({"run", "--seccomp", "on", fixture_rootfs(), "--", "/bin/echo", "ok"});
    CHECK_EQ(on.shell_status, 0);
    CHECK_EQ(trim(on.out), "ok");
}
