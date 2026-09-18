#!/usr/bin/env python3
"""Generates src/linux_abi/syscalls.def from the classification below.

Development tool only (not part of the build). Usage:
  tools/gen_syscall_def.py > src/linux_abi/syscalls.def
  tools/gen_syscall_def.py --check-header /usr/include/x86_64-linux-gnu/asm/unistd_64.h ...

--check-header fails when a header defines a syscall that has no classification,
so a syscall can never silently default to pass-through. Numbers not present in
the generated table are denied with ENOSYS at runtime.
"""
import re
import sys

# name: (class, handler)
# class: pass_through | translated | emulated | denied | unsupported
P = ("pass_through", "none")

T = {
    # --- file I/O on existing descriptors ---
    "read": P, "write": P, "close": P, "lseek": P, "pread64": P, "pwrite64": P,
    "readv": P, "writev": P, "preadv": P, "pwritev": P, "preadv2": P, "pwritev2": P,
    "dup": P, "dup2": P, "dup3": P, "fcntl": P, "flock": P, "fsync": P, "fdatasync": P,
    "ftruncate": P, "fstat": P, "fstatfs": P, "getdents": P, "getdents64": P,
    "fchdir": P, "sendfile": P, "splice": P, "tee": P, "vmsplice": P,
    "copy_file_range": P, "sync_file_range": P, "fallocate": P, "fadvise64": P,
    "readahead": P, "sync": P, "syncfs": P, "ioctl": P, "pipe": P, "pipe2": P,
    "close_range": P, "fgetxattr": P, "flistxattr": P, "umask": P,
    "select": P, "pselect6": P, "poll": P, "ppoll": P,
    "epoll_create": P, "epoll_create1": P, "epoll_ctl": P, "epoll_wait": P,
    "epoll_pwait": P, "epoll_pwait2": P, "epoll_ctl_old": ("denied", "deny_enosys"),
    "epoll_wait_old": ("denied", "deny_enosys"),
    "eventfd": P, "eventfd2": P, "signalfd": P, "signalfd4": P,
    "timerfd_create": P, "timerfd_settime": P, "timerfd_gettime": P,
    "inotify_init": P, "inotify_init1": P, "inotify_rm_watch": P, "fanotify_init": P,
    "memfd_create": P, "memfd_secret": P, "userfaultfd": P, "cachestat": P,
    # --- memory ---
    "mmap": P, "mprotect": P, "munmap": P, "brk": P, "mremap": P, "msync": P,
    "mincore": P, "madvise": P, "mlock": P, "munlock": P, "mlockall": P, "munlockall": P,
    "mlock2": P, "remap_file_pages": P, "mbind": P, "set_mempolicy": P, "get_mempolicy": P,
    "migrate_pages": P, "move_pages": P, "pkey_mprotect": P, "pkey_alloc": P, "pkey_free": P,
    "set_mempolicy_home_node": P, "map_shadow_stack": P, "mseal": P,
    "process_madvise": P, "process_mrelease": P,
    # --- signals / time / scheduling ---
    "rt_sigaction": P, "rt_sigprocmask": P, "rt_sigreturn": P, "rt_sigpending": P,
    "rt_sigtimedwait": P, "rt_sigsuspend": P, "sigaltstack": P, "pause": P,
    "nanosleep": P, "clock_nanosleep": P, "getitimer": P, "setitimer": P, "alarm": P,
    "gettimeofday": P, "settimeofday": P, "time": P, "times": P, "clock_gettime": P,
    "clock_settime": P, "clock_getres": P, "clock_adjtime": P, "adjtimex": P,
    "timer_create": P, "timer_settime": P, "timer_gettime": P, "timer_getoverrun": P,
    "timer_delete": P, "sched_yield": P, "sched_setparam": P, "sched_getparam": P,
    "sched_setscheduler": P, "sched_getscheduler": P, "sched_get_priority_max": P,
    "sched_get_priority_min": P, "sched_rr_get_interval": P, "sched_setaffinity": P,
    "sched_getaffinity": P, "sched_setattr": P, "sched_getattr": P,
    "getpriority": P, "setpriority": P, "ioprio_set": P, "ioprio_get": P,
    "futex": P, "futex_waitv": P, "futex_wake": P, "futex_wait": P, "futex_requeue": P,
    "set_robust_list": P, "get_robust_list": P, "restart_syscall": P, "rseq": P,
    # --- process ---
    "fork": P, "vfork": P, "clone": ("translated", "clone"),
    "clone3": ("denied", "deny_enosys"),
    "exit": P, "exit_group": P, "wait4": P, "waitid": P, "getpid": P, "getppid": P,
    "gettid": P, "getpgrp": P, "getpgid": P, "setpgid": P, "getsid": P, "setsid": P,
    "set_tid_address": P, "prctl": P, "arch_prctl": P, "personality": P,
    "getrlimit": P, "setrlimit": P, "prlimit64": P, "getrusage": P, "sysinfo": P,
    "uname": P, "getcpu": P, "getrandom": P, "capget": P, "capset": P, "seccomp": P,
    "kcmp": P, "process_vm_readv": P, "process_vm_writev": P, "modify_ldt": P,
    "set_thread_area": P, "get_thread_area": P, "vhangup": P, "syslog": P,
    "sysfs": P, "ustat": P, "uretprobe": P,
    "landlock_create_ruleset": P, "landlock_add_rule": P, "landlock_restrict_self": P,
    "lsm_get_self_attr": P, "lsm_set_self_attr": P, "lsm_list_modules": P,
    "statmount": P, "listmount": P, "membarrier": P,
    "execve": ("translated", "execve"), "execveat": ("translated", "execveat"),
    "kill": ("translated", "kill"), "tkill": ("translated", "tkill"),
    "tgkill": ("translated", "tgkill"),
    "rt_sigqueueinfo": ("translated", "sigqueue"),
    "rt_tgsigqueueinfo": ("translated", "tgsigqueue"),
    "pidfd_open": ("translated", "pidfd_open"), "pidfd_send_signal": P,
    "pidfd_getfd": ("denied", "deny_eperm"),
    "ptrace": ("unsupported", "deny_eperm"),
    # --- identity ---
    "getuid": ("emulated", "getuid"), "geteuid": ("emulated", "geteuid"),
    "getgid": ("emulated", "getgid"), "getegid": ("emulated", "getegid"),
    "getresuid": ("emulated", "getresuid"), "getresgid": ("emulated", "getresgid"),
    "getgroups": P,
    "setuid": ("emulated", "setid"), "setgid": ("emulated", "setid"),
    "setreuid": ("emulated", "setid"), "setregid": ("emulated", "setid"),
    "setresuid": ("emulated", "setid"), "setresgid": ("emulated", "setid"),
    "setfsuid": ("emulated", "setid"), "setfsgid": ("emulated", "setid"),
    "setgroups": ("emulated", "setid"),
    # --- path based filesystem ---
    "open": ("translated", "open"), "openat": ("translated", "openat"),
    "openat2": ("unsupported", "deny_enosys"), "creat": ("translated", "creat"),
    "stat": ("translated", "stat"), "lstat": ("translated", "lstat"),
    "newfstatat": ("translated", "newfstatat"), "statx": ("translated", "statx"),
    "access": ("translated", "access"), "faccessat": ("translated", "faccessat"),
    "faccessat2": ("translated", "faccessat2"),
    "readlink": ("translated", "readlink"), "readlinkat": ("translated", "readlinkat"),
    "chdir": ("translated", "chdir"), "getcwd": ("translated", "getcwd"),
    "mkdir": ("translated", "mkdir"), "mkdirat": ("translated", "mkdirat"),
    "mknod": ("translated", "mknod"), "mknodat": ("translated", "mknodat"),
    "rmdir": ("translated", "rmdir"), "unlink": ("translated", "unlink"),
    "unlinkat": ("translated", "unlinkat"), "rename": ("translated", "rename"),
    "renameat": ("translated", "renameat"), "renameat2": ("translated", "renameat2"),
    "link": ("translated", "link"), "linkat": ("translated", "linkat"),
    "symlink": ("translated", "symlink"), "symlinkat": ("translated", "symlinkat"),
    "chmod": ("translated", "chmod"), "fchmodat": ("translated", "fchmodat"),
    "fchmodat2": ("translated", "fchmodat2"), "fchmod": ("translated", "fd_meta_write"),
    "chown": ("translated", "chown"), "lchown": ("translated", "lchown"),
    "fchownat": ("translated", "fchownat"), "fchown": ("translated", "fchown_fd"),
    "utime": ("translated", "utime"), "utimes": ("translated", "utimes"),
    "futimesat": ("translated", "futimesat"), "utimensat": ("translated", "utimensat"),
    "truncate": ("translated", "truncate"), "statfs": ("translated", "statfs"),
    "getxattr": ("translated", "xattr_get"), "listxattr": ("translated", "xattr_get"),
    "lgetxattr": ("translated", "xattr_lget"), "llistxattr": ("translated", "xattr_lget"),
    "setxattr": ("translated", "xattr_set"), "removexattr": ("translated", "xattr_set"),
    "lsetxattr": ("translated", "xattr_lset"), "lremovexattr": ("translated", "xattr_lset"),
    "fsetxattr": ("translated", "fd_meta_write"), "fremovexattr": ("translated", "fd_meta_write"),
    "inotify_add_watch": ("translated", "inotify_add_watch"),
    "fanotify_mark": ("translated", "fanotify_mark"),
    "name_to_handle_at": ("unsupported", "deny_eopnotsupp"),
    "open_by_handle_at": ("denied", "deny_eperm"),
    "uselib": ("unsupported", "deny_enosys"),
    "chroot": ("unsupported", "deny_eperm"),
    # --- sockets / IPC ---
    "socket": ("translated", "socket"), "socketpair": P,
    "connect": ("translated", "connect"), "bind": ("translated", "bind"),
    "sendto": ("translated", "sendto"), "sendmsg": ("translated", "sendmsg"),
    "sendmmsg": ("translated", "sendmmsg"),
    "accept": P, "accept4": P, "recvfrom": P, "recvmsg": P, "recvmmsg": P,
    "shutdown": P, "listen": P, "getsockname": P, "getpeername": P,
    "setsockopt": P, "getsockopt": P,
    "shmget": P, "shmat": P, "shmctl": P, "shmdt": P, "semget": P, "semop": P,
    "semctl": P, "semtimedop": P, "msgget": P, "msgsnd": P, "msgrcv": P, "msgctl": P,
    "mq_open": P, "mq_unlink": P, "mq_timedsend": P, "mq_timedreceive": P,
    "mq_notify": P, "mq_getsetattr": P,
    "add_key": P, "request_key": P, "keyctl": P,
    "io_setup": P, "io_destroy": P, "io_getevents": P, "io_submit": P, "io_cancel": P,
    "io_pgetevents": P,
    # io_uring executes path operations in the kernel without syscall stops.
    "io_uring_setup": ("denied", "deny_enosys"), "io_uring_enter": ("denied", "deny_enosys"),
    "io_uring_register": ("denied", "deny_enosys"),
    # --- namespaces, mounts, privileged or obsolete ---
    "unshare": ("denied", "deny_eperm"), "setns": ("denied", "deny_eperm"),
    "mount": ("denied", "deny_eperm"), "umount2": ("denied", "deny_eperm"),
    "pivot_root": ("denied", "deny_eperm"), "open_tree": ("denied", "deny_eperm"),
    "move_mount": ("denied", "deny_eperm"), "fsopen": ("denied", "deny_eperm"),
    "fsconfig": ("denied", "deny_eperm"), "fsmount": ("denied", "deny_eperm"),
    "fspick": ("denied", "deny_eperm"), "mount_setattr": ("denied", "deny_eperm"),
    "swapon": ("denied", "deny_eperm"), "swapoff": ("denied", "deny_eperm"),
    "acct": ("denied", "deny_eperm"), "reboot": ("denied", "deny_eperm"),
    "sethostname": ("denied", "deny_eperm"), "setdomainname": ("denied", "deny_eperm"),
    "iopl": ("denied", "deny_eperm"), "ioperm": ("denied", "deny_eperm"),
    "kexec_load": ("denied", "deny_eperm"), "kexec_file_load": ("denied", "deny_eperm"),
    "init_module": ("denied", "deny_eperm"), "finit_module": ("denied", "deny_eperm"),
    "delete_module": ("denied", "deny_eperm"), "quotactl": ("denied", "deny_eperm"),
    "quotactl_fd": ("denied", "deny_eperm"), "bpf": ("denied", "deny_eperm"),
    "perf_event_open": ("denied", "deny_eacces"), "lookup_dcookie": ("denied", "deny_enosys"),
    "create_module": ("denied", "deny_enosys"), "get_kernel_syms": ("denied", "deny_enosys"),
    "query_module": ("denied", "deny_enosys"), "nfsservctl": ("denied", "deny_enosys"),
    "getpmsg": ("denied", "deny_enosys"), "putpmsg": ("denied", "deny_enosys"),
    "afs_syscall": ("denied", "deny_enosys"), "tuxcall": ("denied", "deny_enosys"),
    "security": ("denied", "deny_enosys"), "vserver": ("denied", "deny_enosys"),
    "_sysctl": ("denied", "deny_enosys"),
}

HEADER = """/* Generated by tools/gen_syscall_def.py - do not edit by hand.
 * VHDP_SYSCALL(name, class, handler); entries missing from the target's
 * <asm/unistd.h> are skipped and therefore denied with ENOSYS at runtime. */
"""


def check_header(path):
    names = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"#define __NR_([a-z0-9_]+)\s", line)
            if m:
                names.add(m.group(1))
    missing = sorted(n for n in names if n not in T)
    if missing:
        print(f"{path}: unclassified syscalls: {' '.join(missing)}", file=sys.stderr)
        return 1
    print(f"{path}: {len(names)} syscalls, all classified", file=sys.stderr)
    return 0


def check_names(path):
    """Checks a newline-separated list of names, e.g. produced by preprocessing
    <asm/unistd.h> for a target (asm-generic/unistd.h needs the preprocessor to
    drop 32-bit-only entries):
      echo '#include <asm/unistd.h>' | clang --target=aarch64-linux-android29 \\
        --sysroot=$NDK_SYSROOT -dM -E -x c - | sed -n 's/^#define __NR_\\([a-z0-9_]*\\) .*/\\1/p'
    """
    with open(path, encoding="utf-8") as f:
        names = {line.strip() for line in f if line.strip()}
    missing = sorted(n for n in names if n not in T)
    if missing:
        print(f"{path}: unclassified syscalls: {' '.join(missing)}", file=sys.stderr)
        return 1
    print(f"{path}: {len(names)} syscalls, all classified", file=sys.stderr)
    return 0


def main(argv):
    if len(argv) > 1 and argv[1] == "--check-header":
        return max((check_header(p) for p in argv[2:]), default=0)
    if len(argv) > 1 and argv[1] == "--check-names":
        return max((check_names(p) for p in argv[2:]), default=0)
    out = [HEADER]
    for name in sorted(T):
        cls, handler = T[name]
        out.append(f"#ifdef __NR_{name}\nVHDP_SYSCALL({name}, {cls}, {handler})\n#endif\n")
    sys.stdout.write("".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
