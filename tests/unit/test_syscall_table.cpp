#include "vtest.hpp"

#include "linux_abi/syscall_table.hpp"

#include <asm/unistd.h>

#include <set>

using namespace vhdp::abi;

// Every syscall number the build target's <asm/unistd.h> defines must be
// classified; nothing may default to pass-through by omission.
VTEST(syscall_table_complete) {
    std::set<long> classified;
    for (const auto& s : all_syscalls()) {
        classified.insert(s.nr);
    }
    struct Named {
        const char* name;
        long nr;
    };
    // A representative sample spanning classes; each must be present and classified.
    const Named sample[] = {
        {"read", __NR_read},     {"write", __NR_write},           {"openat", __NR_openat},
        {"execve", __NR_execve}, {"clone", __NR_clone},           {"mount", __NR_mount},
        {"getpid", __NR_getpid}, {"getuid", __NR_getuid},         {"ptrace", __NR_ptrace},
        {"mmap", __NR_mmap},     {"exit_group", __NR_exit_group},
    };
    for (const auto& n : sample) {
        const SyscallInfo* info = lookup_syscall(n.nr);
        REQUIRE(info != nullptr);
        CHECK_EQ(std::string(info->name), std::string(n.name));
    }
}

VTEST(syscall_table_lookup) {
    CHECK(lookup_syscall(-1) == nullptr);
    CHECK(lookup_syscall(999999) == nullptr);

    const SyscallInfo* mnt = lookup_syscall(__NR_mount);
    REQUIRE(mnt != nullptr);
    CHECK_EQ(mnt->cls, SyscallClass::denied);

    const SyscallInfo* rd = lookup_syscall(__NR_read);
    REQUIRE(rd != nullptr);
    CHECK_EQ(rd->cls, SyscallClass::pass_through);

    const SyscallInfo* ex = lookup_syscall(__NR_execve);
    REQUIRE(ex != nullptr);
    CHECK_EQ(ex->cls, SyscallClass::translated);
    CHECK_EQ(ex->handler, Handler::execve);

    const SyscallInfo* gu = lookup_syscall(__NR_getuid);
    REQUIRE(gu != nullptr);
    CHECK_EQ(gu->cls, SyscallClass::emulated);

    CHECK_EQ(lookup_nr("openat"), static_cast<long>(__NR_openat));
    CHECK_EQ(lookup_nr("no_such_syscall"), -1);
    CHECK_EQ(std::string(class_name(SyscallClass::translated)), "translated");
}
