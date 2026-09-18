#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(net_host_socket) {
    // Default --network host: creating an INET socket succeeds.
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/try", "socket-inet"});
    CHECK_NE(r.out.find("ok"), std::string::npos);
}

VTEST(net_none) {
    // --network none: INET socket refused (EACCES), AF_UNIX still allowed.
    RunResult inet =
        run_phdp({"run", "--network", "none", fixture_rootfs(), "--", "/bin/try", "socket-inet"});
    CHECK_NE(inet.out.find("EACCES"), std::string::npos);
    RunResult unix_s =
        run_phdp({"run", "--network", "none", fixture_rootfs(), "--", "/bin/try", "socket-unix"});
    CHECK_NE(unix_s.out.find("ok"), std::string::npos);
    // Works under both syscall paths.
    RunResult inet_pt = run_phdp({"run", "--network", "none", "--seccomp", "off", fixture_rootfs(),
                                  "--", "/bin/try", "socket-inet"});
    CHECK_NE(inet_pt.out.find("EACCES"), std::string::npos);
}
