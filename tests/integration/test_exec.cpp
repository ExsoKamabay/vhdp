#include <cstdlib>
#include "harness.hpp"
#include "vtest.hpp"

using namespace it;

VTEST(exec_dynamic_loader) {
    if (!have_dynamic_fixture()) {
        VTEST_SKIP("dynamic fixture unavailable on this host (loader could not be populated)");
    }
    // A dynamic ELF runs via loader indirection (the guest's own ld.so).
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/vhdp-fixture-dyn", "echo", "dynamic-ok"});
    CHECK_EQ(r.shell_status, 0);
    CHECK_EQ(trim(r.out), "dynamic-ok");
    RunResult exe = run_phdp(
        {"run", "--proc", "host", fixture_rootfs(), "--", "/bin/vhdp-fixture-dyn", "self-exe"});
    if (std::getenv("VHDP_LOADER") != nullptr) {
        // Userland loader mode: /proc/self/exe names the guest program itself.
        CHECK_EQ(trim(exe.out), "/bin/vhdp-fixture-dyn");
    } else {
        // ld.so indirection: /proc/self/exe names the dynamic loader (documented).
        CHECK_NE(exe.out.find("ld-"), std::string::npos);
    }
}

VTEST(exec_shebang) {
    // /bin/script.sh is "#!/bin/echo shebang-arg"; running it prints the
    // interpreter argument followed by the script path.
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/script.sh"});
    CHECK_EQ(r.shell_status, 0);
    CHECK_NE(r.out.find("shebang-arg"), std::string::npos);
    CHECK_NE(r.out.find("/bin/script.sh"), std::string::npos);
}

VTEST(exec_abi_mismatch) {
    // /bin/foreign is a valid ELF for the other architecture.
    RunResult r = run_phdp({"run", "--json", fixture_rootfs(), "--", "/bin/foreign"});
    CHECK_EQ(r.shell_status, 126); // ENOEXEC -> 126
    CHECK_NE(r.err.find("abi_mismatch"), std::string::npos);
}

VTEST(exec_loader_missing) {
    // The min rootfs has the dynamic binary but no loader.
    if (!have_dynamic_fixture()) {
        VTEST_SKIP("dynamic fixture unavailable on this host");
    }
    RunResult r =
        run_phdp({"run", "--json", fixture_min_rootfs(), "--", "/bin/no-loader", "echo", "x"});
    CHECK_NE(r.shell_status, 0);
    CHECK_NE(r.err.find("loader"), std::string::npos);
}

VTEST(exec_not_found) {
    RunResult miss = run_phdp({fixture_rootfs(), "--", "/bin/does-not-exist"});
    CHECK_EQ(miss.shell_status, 127); // ENOENT -> 127
    RunResult noexec = run_phdp({fixture_rootfs(), "--", "/bin/not-elf"});
    CHECK_EQ(noexec.shell_status, 126); // ENOEXEC -> 126
    RunResult path_miss = run_phdp({fixture_rootfs(), "--", "totally-not-a-command"});
    CHECK_EQ(path_miss.shell_status, 127);
}
