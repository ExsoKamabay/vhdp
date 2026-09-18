# Matriks Kemampuan VHDP

Sumber kebenarannya adalah `phdp capabilities --json`, yang dibangun dari
`src/core/capabilities.cpp`. Dokumen ini menyalin isinya. Teks deskripsi
dibiarkan dalam bahasa Inggris karena sama persis dengan output program. Setiap
klaim `supported` atau `partial` untuk profil `linux-x86_64` menyebut ID test
yang mengujinya; jalankan `ctest --preset host-debug` untuk memeriksa.

Nilai status: `supported`, `partial`, `unsupported`, `untested`,
`backend-required`.

## Profil eksekusi

Profil dipisah karena host yang berbeda memberi izin yang berbeda.

| profil | status | deskripsi dan bukti |
|---|---|---|
| `linux-x86_64` | supported | Linux x86_64 host, same-architecture guest, rootless engine (development/CI profile; not an Android claim). host test suite (ctest) on Linux x86_64 |
| `linux-aarch64` | untested | Linux aarch64 host, same-architecture guest, rootless engine. compiled for Android arm64 only; no aarch64 Linux test run |
| `terminal-unprivileged` | untested | Android ARM64 CLI from a terminal app or adb shell, same-architecture guest, when the ptrace probe passes. android-arm64-release artifacts build with the official NDK; device tests NOT RUN |
| `android-emulator-x86_64` | untested | Android x86_64 emulator CLI, same-architecture guest. android-x86_64-debug artifacts build; emulator tests NOT RUN |
| `android-app` | supported | libvhdp inside a third-party Android app (targetSdk >= 29). guest programs start through the userland loader (libvhdp-loader.so in nativeLibraryDir), which maps them PROT_EXEC from app storage; host policy refusals are answered (seccomp traps, link(2), NETLINK_AUDIT, denied /proc files). Device QA: arm64 Android 15 phone and x86_64 Android 13 running a Debian rootfs through phdp from an app, incl. apt/dpkg, job control and procps |
| `rooted` | backend-required | rooted device with real namespaces/chroot/cgroups. rooted engine not implemented |
| `vm` | backend-required | full guest kernel through a hypervisor (AVF/KVM). VM engine not implemented |
| `cross-architecture` | backend-required | guest ABI different from the host (e.g. x86_64 rootfs on arm64). no emulator adapter integrated; ABI mismatch is detected and reported |

Pengujian x86_64 di Android berjalan sebagai aplikasi, jadi hasilnya masuk
profil `android-app`, bukan `android-emulator-x86_64`.

## Fitur

Kolom `terminal-unprivileged`, `rooted`, dan `vm` tidak ditampilkan di sini;
lihat `phdp capabilities` untuk tabel lengkapnya.

| fitur | linux-x86_64 | android-app | test | deskripsi |
|---|---|---|---|---|
| `cli.run` | supported | supported | `T-CLI-RUN-EXIT37`, `T-CLI-POSITIONAL-EXIT37`, `T-CLI-ARGS-ENV-CWD` | phdp run ROOTFS -- CMD and the positional alias; exit status propagation |
| `cli.parser` | supported | supported | `T-UNIT-CLI-ARGS` | subcommand disambiguation, option validation, unsupported options rejected; pure parser, platform independent |
| `cli.default_shell` | supported | unsupported | `T-PTY-DEFAULT-SHELL` | no command runs /bin/sh (fallback /bin/bash, /bin/ash) |
| `cli.reports` | supported | supported | `T-CLI-DOCTOR-JSON`, `T-CLI-INSPECT-JSON`, `T-CLI-CAPABILITIES-JSON` | doctor, inspect, capabilities with --json |
| `cli.json_events` | supported | supported | `T-CLI-JSON-EVENTS` | lifecycle/diagnostic events as JSON Lines on stderr or --event-fd; guest stdout untouched |
| `lib.c_abi` | supported | supported | `T-UNIT-CAPI`, `T-ABI-SYMBOLS`, `T-ABI-HEADERS-C` | stable C ABI: versioned structs, opaque handles, export allowlist, no C++ symbol leaks |
| `lib.cpp_wrapper` | supported | unsupported | `T-UNIT-CPP-WRAPPER`, `T-ABI-HEADERS-CPP` | header-only RAII C++ wrapper over the C ABI |
| `lib.embedding_examples` | supported | unsupported | `T-EX-C-EMBED`, `T-EX-CPP-EMBED` | C and C++ examples run the same session as the CLI |
| `lib.jni_aar` | unsupported | partial | - | JNI/Kotlin wrapper and AAR; no AAR is published; an app embeds the C ABI through its own JNI layer and runs sessions through phdp from nativeLibraryDir |
| `exec.static_elf` | supported | supported | `T-CLI-RUN-EXIT37` | static same-architecture ELF from the rootfs |
| `exec.dynamic_elf` | supported | supported | `T-EXEC-DYNAMIC-LOADER` | dynamic ELF through the rootfs dynamic loader (loader indirection); without the userland loader /proc/self/exe names ld.so (glibc --argv0 used when supported); with it (Android) exe names the program |
| `exec.shebang` | supported | supported | `T-EXEC-SHEBANG` | #! scripts with interpreter lookup inside the rootfs |
| `exec.abi_mismatch` | supported | unsupported | `T-EXEC-ABI-MISMATCH` | foreign-architecture ELF refused with ENOEXEC and a diagnostic |
| `exec.loader_missing` | supported | unsupported | `T-EXEC-LOADER-MISSING` | missing PT_INTERP loader refused with ENOENT and a diagnostic |
| `fs.paths` | supported | supported | `T-FS-ABS-REL`, `T-UNIT-RESOLVER` | absolute and relative paths, cwd, chdir/getcwd |
| `fs.at_family` | partial | partial | `T-FS-DIRFD` | dirfd-relative *at syscalls (openat, newfstatat, statx, mkdirat, unlinkat, renameat2, linkat, symlinkat, readlinkat, faccessat2, fchmodat, fchownat, utimensat); openat2 refused with ENOSYS |
| `fs.dotdot` | supported | supported | `T-FS-DOTDOT`, `T-UNIT-RESOLVER` | '..' never climbs above the guest root |
| `fs.symlinks` | supported | supported | `T-FS-SYMLINK-ESCAPE`, `T-UNIT-RESOLVER` | absolute/relative symlinks resolved in guest space; loops give ELOOP; resolution/use race (TOCTOU) remains; see SECURITY.md |
| `fs.hardlink_rename` | partial | supported | `T-FS-RENAME-LINK` | link/rename inside a mount; EXDEV across mounts; where the host refuses link(2) (Android app storage) hard links are emulated: shared object, st_nlink/st_ino, d_type, collapse back to a file at one name |
| `fs.magic_links` | partial | partial | `T-FS-MAGIC-LINK` | /proc magic links (--proc host): self remap, outside targets fail closed; host /proc content is visible when --proc host is used; files the host refuses (Android: stat, uptime, loadavg, version, vmstat, ...) are served by stand-ins, and /proc/<pid>/status of session processes shows the guest identity |
| `fs.binds` | supported | supported | `T-FS-BIND-RO`, `T-FS-BIND-RW` | explicit binds, read-only by default, :rw opt-in; bind targets missing in the rootfs are not listed by readdir |
| `fs.read_only_rootfs` | supported | unsupported | `T-FS-READONLY-ROOTFS` | --read-only-rootfs returns EROFS for writes |
| `fs.inherited_fd` | supported | unsupported | `T-FS-INHERITED-FD` | only stdio is inherited; descriptors outside the mounts are refused as dirfd |
| `fs.unix_sockets` | partial | unsupported | `T-FS-UNIX-SOCKET` | AF_UNIX pathname connect/bind/sendto/sendmsg translated; host path must fit 107 bytes; sendmmsg with pathname addresses refused |
| `identity.uid_gid` | partial | partial | `T-ID-UID-GID` | guest-visible uid/gid and stat ownership (--uid/--gid); credentials tracked per process (set*id rules, getgroups/setgroups); chown is a no-op for uid 0 |
| `dev.minimal` | supported | supported | `T-DEV-MINIMAL` | /dev/null, zero, full, random, urandom, tty, ptmx, pts |
| `tty.pty` | supported | supported | `T-PTY-CTRL-C`, `T-PTY-RESIZE`, `T-LIB-PTY-RESIZE` | PTY sessions, Ctrl-C/SIGINT exit 130, window resize |
| `tty.job_control` | partial | supported | `T-PTY-JOB-CONTROL` | interactive shell job control; test drives the fixture shell on a PTY (command list and exit status); stopping and resuming jobs is not covered by an automated test |
| `proc.signals` | supported | supported | `T-CLI-SIGNAL-FORWARD` | signal forwarding and 128+N exit status; signals to processes outside the session are refused (ESRCH) |
| `proc.tree_cleanup` | supported | unsupported | `T-LIFE-TREE-CANCEL`, `T-LIFE-100-CYCLES` | whole process tree killed on exit/cancel; no zombies or leaked fds |
| `proc.timeout` | supported | unsupported | `T-CLI-TIMEOUT` | --timeout kills the tree and exits 124 |
| `proc.multiprocess` | supported | supported | `T-PROC-FANOUT` | fork/vfork/clone/threads followed; CLONE_UNTRACED stripped; clone3 -> ENOSYS |
| `syscall.policy` | supported | supported | `T-SYS-UNSUPPORTED`, `T-UNIT-SYSCALL-TABLE` | unknown and dangerous syscalls refused with errno + diagnostic |
| `syscall.seccomp_acceleration` | supported | supported | `T-UNIT-SECCOMP-FILTER`, `T-SYS-PTRACE-ONLY` | seccomp RET_TRACE accelerator with ptrace fallback |
| `net.host` | supported | supported | `T-NET-HOST-SOCKET` | host network pass-through; socket creation tested; no external traffic in tests |
| `net.none` | partial | unsupported | `T-NET-NONE` | --network none refuses non-AF_UNIX sockets; abstract AF_UNIX sockets stay reachable |
| `net.dns` | untested | supported | - | DNS resolution inside the guest; depends on rootfs resolv.conf; no test |
| `guest.kernel_modules` | unsupported | unsupported | - | kernel modules / guest kernel drivers |
| `guest.systemd_cgroups` | unsupported | unsupported | - | systemd, full cgroups |
| `guest.containers_kvm_ebpf` | unsupported | unsupported | - | nested containers, KVM, eBPF, netfilter, FUSE |
| `guest.setuid_privilege` | unsupported | unsupported | - | setuid/capabilities granting host privilege; no_new_privs is always set |
| `guest.hardware` | unsupported | unsupported | - | USB, raw block devices, GPU passthrough, GUI acceleration |
| `security.sandbox` | unsupported | unsupported | - | isolation for hostile root filesystems; rootless path translation is compatibility isolation only; use an isolated VM |

## Klasifikasi syscall

`phdp capabilities --json` juga menghitung syscall per kelas untuk arsitektur
build. Di x86_64: 235 pass-through, 69 translated, 15 emulated, 49 denied, 5
unsupported. Nomor yang tidak ada di tabel ditolak dengan `ENOSYS`.

## Menguji di perangkat Android

Host pengembangan adalah x86_64, jadi biner arm64 hanya bisa dijalankan di
perangkat. Profil `terminal-unprivileged` tetap `untested` sampai langkah
berikut dijalankan dan hasilnya dicatat:

```sh
cmake --preset android-arm64-release
cmake --build --preset android-arm64-release --parallel
adb push build/android-arm64-release/src/phdp /data/local/tmp/phdp
adb shell /data/local/tmp/phdp doctor
adb shell /data/local/tmp/phdp /data/local/tmp/rootfs -- /bin/sh -lc 'id; uname -a'
```

Rootfs arm64 disiapkan seperti di [ROOTFS.md](ROOTFS.md).
