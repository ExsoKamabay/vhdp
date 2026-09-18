#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(pty_ctrl_c) {
    // Guest on a PTY; sending Ctrl-C (0x03) delivers SIGINT -> exit 130.
    RunResult r = run_phdp_pty({"run", "--pty", fixture_rootfs(), "--", "/bin/ready-sleep", "10"},
                               std::string(1, '\x03'), 8000);
    CHECK(!r.timed_out);
    CHECK_EQ(r.shell_status, 130);
}

VTEST(pty_resize) {
    // The guest reports the initial 24x80 window set by the PTY.
    RunResult r = run_phdp_pty({"run", "--pty", fixture_rootfs(), "--", "/bin/winsize"}, "", 8000);
    CHECK(!r.timed_out);
    CHECK_NE(r.out.find("winsize 24 80"), std::string::npos);
}

VTEST(pty_job_control) {
    // Interactive shell on a PTY runs a pipeline and job-control-free command
    // list, then exits with the last status.
    RunResult r = run_phdp_pty({"run", "--pty", fixture_rootfs(), "--", "/bin/sh"},
                               "echo one\ntrue-or-not\nexit 3\n", 8000);
    CHECK(!r.timed_out);
    CHECK_NE(r.out.find("one"), std::string::npos);
    CHECK_EQ(r.shell_status, 3);
}
