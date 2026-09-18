#include "harness.hpp"
#include "vtest.hpp"

#include <string>

using namespace it;

VTEST(fs_abs_rel) {
    // Absolute path.
    RunResult a = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/hello.txt"});
    CHECK_EQ(trim(a.out), "hello-content");
    // Relative path resolved against --cwd.
    RunResult rel =
        run_phdp({"run", "--cwd", "/etc", fixture_rootfs(), "--", "/bin/cat", "hello.txt"});
    CHECK_EQ(trim(rel.out), "hello-content");
    // chdir then getcwd sees the guest path.
    RunResult cd = run_phdp({fixture_rootfs(), "--", "/bin/chdir-pwd", "/sub/deep"});
    CHECK_EQ(trim(cd.out), "/sub/deep");
}

VTEST(fs_dirfd) {
    // openat relative to a directory fd inside the rootfs.
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/try", "openat", "/etc", "hello.txt"});
    CHECK_NE(r.out.find("ok"), std::string::npos);
    CHECK_NE(r.out.find("hello-content"), std::string::npos);
}

VTEST(fs_dotdot) {
    // '..' cannot escape: /etc/../etc/hello.txt stays inside; escape attempts ENOENT.
    RunResult ok = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/../etc/hello.txt"});
    CHECK_EQ(trim(ok.out), "hello-content");
    RunResult esc =
        run_phdp({fixture_rootfs(), "--", "/bin/try", "open-r", "/../../../../../../etc/shadow"});
    CHECK_NE(esc.out.find("ENOENT"), std::string::npos);
}

VTEST(fs_symlink_escape) {
    // Absolute symlink /etc/abs-link -> /etc/hello.txt resolves inside the rootfs.
    RunResult abs = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/abs-link"});
    CHECK_EQ(trim(abs.out), "hello-content");
    RunResult rel = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/rel-link"});
    CHECK_EQ(trim(rel.out), "hello-content");
    // The escape symlink points at /../../../../etc/passwd, clamped to the rootfs
    // where it does not exist.
    RunResult esc = run_phdp({fixture_rootfs(), "--", "/bin/cat", "/etc/escape-link"});
    CHECK_NE(esc.err.find("ENOENT"), std::string::npos);
    // Symlink loop -> ELOOP.
    RunResult loop = run_phdp({fixture_rootfs(), "--", "/bin/try", "open-r", "/etc/loop-a"});
    CHECK_NE(loop.out.find("ELOOP"), std::string::npos);
}

VTEST(fs_rename_link) {
    ScratchRootfs s;
    REQUIRE(s.ok());
    RunResult mk = run_phdp({s.path(), "--", "/bin/try", "create", "/tmp/a.txt", "data"});
    CHECK_NE(mk.out.find("ok"), std::string::npos);
    RunResult mv = run_phdp({s.path(), "--", "/bin/try", "rename", "/tmp/a.txt", "/tmp/b.txt"});
    CHECK_NE(mv.out.find("ok"), std::string::npos);
    CHECK(file_contains(s.path() + "/tmp/b.txt", "data"));
    RunResult ln = run_phdp({s.path(), "--", "/bin/try", "link", "/tmp/b.txt", "/tmp/c.txt"});
    CHECK_NE(ln.out.find("ok"), std::string::npos);
    // Cross-mount rename would be EXDEV, but there is only one writable mount here;
    // rename onto a read-only device mount is refused.
    RunResult xdev = run_phdp({s.path(), "--", "/bin/try", "rename", "/tmp/b.txt", "/dev/null/x"});
    CHECK(xdev.out.find("ok") == std::string::npos);
}

VTEST(fs_bind_ro) {
    ScratchRootfs s;
    REQUIRE(s.ok());
    // Create a host directory to bind.
    std::string host = s.path() + "/tmp/hostdir";
    int rc =
        std::system(("mkdir -p '" + host + "' && echo bounddata > '" + host + "/f.txt'").c_str());
    (void)rc;
    RunResult ro =
        run_phdp({"run", "--bind", host + ":/mnt", s.path(), "--", "/bin/cat", "/mnt/f.txt"});
    CHECK_EQ(trim(ro.out), "bounddata");
    // Read-only by default: writing fails.
    RunResult w = run_phdp({"run", "--bind", host + ":/mnt", s.path(), "--", "/bin/try", "create",
                            "/mnt/new.txt", "x"});
    CHECK(w.out.find("ok") == std::string::npos);
    CHECK_NE(w.out.find("EROFS"), std::string::npos);
}

VTEST(fs_bind_rw) {
    ScratchRootfs s;
    REQUIRE(s.ok());
    std::string host = s.path() + "/tmp/rwdir";
    int rc2 = std::system(("mkdir -p '" + host + "'").c_str());
    (void)rc2;
    RunResult w = run_phdp({"run", "--bind", host + ":/mnt:rw", s.path(), "--", "/bin/try",
                            "create", "/mnt/new.txt", "written"});
    CHECK_NE(w.out.find("ok"), std::string::npos);
    CHECK(file_contains(host + "/new.txt", "written"));
}

VTEST(fs_readonly_rootfs) {
    ScratchRootfs s;
    REQUIRE(s.ok());
    RunResult w = run_phdp(
        {"run", "--read-only-rootfs", s.path(), "--", "/bin/try", "create", "/tmp/x.txt", "y"});
    CHECK_NE(w.out.find("EROFS"), std::string::npos);
    // Reads still work.
    RunResult r =
        run_phdp({"run", "--read-only-rootfs", s.path(), "--", "/bin/cat", "/etc/hello.txt"});
    CHECK_EQ(trim(r.out), "hello-content");
}

VTEST(fs_inherited_fd) {
    // A descriptor number that phdp does not pass to the guest must not be usable
    // as a dirfd. fd 3+ are closed in the guest (only stdio inherited).
    RunResult r = run_phdp({fixture_rootfs(), "--", "/bin/try", "fd-openat", "3", "etc/hello.txt"});
    CHECK_NE(r.out.find("closed"), std::string::npos);
    // Confirm only 0,1,2 are open.
    RunResult fds = run_phdp({fixture_rootfs(), "--", "/bin/check-fds"});
    CHECK_EQ(trim(fds.out), "0\n1\n2");
}

VTEST(fs_unix_socket) {
    ScratchRootfs s;
    REQUIRE(s.ok());
    // AF_UNIX pathname bind+connect round-trip; the socket path is translated.
    RunResult r = run_phdp({s.path(), "--", "/bin/unix-roundtrip", "/tmp/sock"});
    CHECK_NE(r.out.find("unix ok"), std::string::npos);
    CHECK_NE(r.out.find("socket-visible"), std::string::npos);
}
