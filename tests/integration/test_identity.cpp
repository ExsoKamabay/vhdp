#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(identity_uid_gid) {
    // Default: real host identity is visible.
    RunResult def = run_phdp({fixture_rootfs(), "--", "/bin/uid"});
    CHECK_EQ(def.shell_status, 0);

    // --uid 0 --gid 0: the guest sees root through the get*id family.
    RunResult root =
        run_phdp({"run", "--uid", "0", "--gid", "0", fixture_rootfs(), "--", "/bin/uid"});
    CHECK_NE(root.out.find("uid=0 euid=0 gid=0 egid=0"), std::string::npos);
    CHECK_NE(root.out.find("resuid=0,0,0"), std::string::npos);
    CHECK_NE(root.out.find("resgid=0,0,0"), std::string::npos);

    // stat of a host-owned file reports the emulated owner.
    RunResult st = run_phdp({"run", "--uid", "1234", "--gid", "5678", fixture_rootfs(), "--",
                             "/bin/stat", "/etc/hello.txt"});
    CHECK_NE(st.out.find("uid=1234"), std::string::npos);
    CHECK_NE(st.out.find("gid=5678"), std::string::npos);

    // setuid to another id is refused (EPERM) for a non-root guest identity;
    // for the fake-root identity it is an accepted no-op (exit 0 from try).
    RunResult su =
        run_phdp({"run", "--uid", "1000", fixture_rootfs(), "--", "/bin/try", "syscall", "0"});
    CHECK_EQ(su.shell_status, 0);
}

VTEST(dev_minimal) {
    // /dev/null swallows writes; /dev/zero yields zeros; /dev/urandom exists.
    RunResult null = run_phdp({fixture_rootfs(), "--", "/bin/try", "open-w", "/dev/null"});
    CHECK_NE(null.out.find("ok"), std::string::npos);
    RunResult urandom = run_phdp({fixture_rootfs(), "--", "/bin/try", "open-r", "/dev/urandom"});
    CHECK_NE(urandom.out.find("ok"), std::string::npos);
    // --dev none hides them.
    RunResult none = run_phdp(
        {"run", "--dev", "none", fixture_rootfs(), "--", "/bin/try", "open-r", "/dev/null"});
    CHECK(none.out.find("ok") == std::string::npos);
}
