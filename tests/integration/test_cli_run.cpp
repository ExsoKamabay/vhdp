#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(cli_run_exit37) {
    RunResult r = run_phdp({"run", fixture_rootfs(), "--", "/bin/exit", "37"});
    CHECK_EQ(r.shell_status, 37);
    RunResult z = run_phdp({"run", fixture_rootfs(), "--", "/bin/echo", "hello", "world"});
    CHECK_EQ(z.shell_status, 0);
    CHECK_EQ(trim(z.out), "hello world");
}

VTEST(cli_positional_exit37) {
    // Positional alias (no `run`) must behave identically.
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/exit", "37"});
    CHECK_EQ(r.shell_status, 37);
    RunResult c = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/hello.txt"});
    CHECK_EQ(c.shell_status, 0);
    CHECK_EQ(trim(c.out), "hello-content");
}

VTEST(cli_args_env_cwd) {
    // PATH search: command name without a slash is found in the guest PATH.
    RunResult path = run_phdp({fixture_rootfs(), "--", "echo", "found"});
    CHECK_EQ(path.shell_status, 0);
    CHECK_EQ(trim(path.out), "found");

    RunResult env =
        run_phdp({"run", "-e", "FOO=bar", fixture_rootfs(), "--", "/bin/printenv", "FOO"});
    CHECK_EQ(trim(env.out), "bar");

    RunResult cwd = run_phdp({"run", "--cwd", "/etc", fixture_rootfs(), "--", "/bin/pwd"});
    CHECK_EQ(trim(cwd.out), "/etc");

    // Unknown option is rejected (exit 2), nothing executed.
    RunResult bad = run_phdp({"run", "--bogus", fixture_rootfs(), "--", "/bin/echo", "x"});
    CHECK_EQ(bad.shell_status, 2);
}

VTEST(cli_default_shell) {
    // No command: /bin/sh runs and reads from stdin.
    RunResult r = run_phdp({fixture_rootfs()}, "echo shell-alive\nexit 5\n");
    CHECK_EQ(r.shell_status, 5);
    CHECK_NE(r.out.find("shell-alive"), std::string::npos);
}
