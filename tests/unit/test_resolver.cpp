#include "vtest.hpp"

#include "vfs/mount_table.hpp"
#include "vfs/resolver.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include <cerrno>
#include <cstdlib>
#include <string>

using namespace vhdp::vfs;

namespace {

// Creates a disposable rootfs tree; removed by the destructor.
struct Scratch {
    std::string root;
    Scratch() {
        const char* base = std::getenv("TMPDIR");
        std::string tmpl = std::string(base != nullptr ? base : "/tmp") + "/vhdp-resolver-XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        char* d = ::mkdtemp(buf.data());
        root = d != nullptr ? d : "";
    }
    ~Scratch() {
        if (!root.empty()) {
            std::string cmd = "rm -rf '" + root + "'";
            int rc = std::system(cmd.c_str());
            (void)rc;
        }
    }
    void mkdir(const std::string& p) {
        int rc = ::mkdir((root + p).c_str(), 0755);
        (void)rc;
    }
    void file(const std::string& p) {
        int fd = ::open((root + p).c_str(), O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            ssize_t n = ::write(fd, "x", 1);
            (void)n;
            ::close(fd);
        }
    }
    void symlink(const std::string& target, const std::string& p) {
        int rc = ::symlink(target.c_str(), (root + p).c_str());
        (void)rc;
    }
};

} // namespace

VTEST(resolver_basic) {
    Scratch s;
    REQUIRE(!s.root.empty());
    s.mkdir("/etc");
    s.file("/etc/hosts");
    MountTable t(s.root, false);

    Resolved r;
    CHECK_EQ(resolve(t, "/etc/hosts", {}, r), 0);
    CHECK(r.exists);
    CHECK(!r.is_dir);
    CHECK_EQ(r.host, s.root + "/etc/hosts");

    CHECK_EQ(resolve(t, "/etc", {}, r), 0);
    CHECK(r.is_dir);

    CHECK_EQ(resolve(t, "/etc/missing", {}, r), 0);
    CHECK(!r.exists);

    // A path component that is a file, used as a directory.
    CHECK_EQ(resolve(t, "/etc/hosts/x", {}, r), ENOTDIR);
    CHECK_EQ(resolve(t, "relative", {}, r), EINVAL);
}

VTEST(resolver_symlink) {
    Scratch s;
    REQUIRE(!s.root.empty());
    s.mkdir("/etc");
    s.file("/etc/real");
    s.symlink("/etc/real", "/etc/abs"); // absolute (guest-space)
    s.symlink("real", "/etc/rel");      // relative
    s.symlink("/etc", "/etc/dir");      // to a dir
    MountTable t(s.root, false);

    Resolved r;
    CHECK_EQ(resolve(t, "/etc/abs", {}, r), 0);
    CHECK(r.exists);
    CHECK_EQ(r.host, s.root + "/etc/real");
    CHECK_EQ(resolve(t, "/etc/rel", {}, r), 0);
    CHECK_EQ(r.host, s.root + "/etc/real");
    CHECK_EQ(resolve(t, "/etc/dir/real", {}, r), 0);
    CHECK(r.exists);

    // follow_final = false leaves the symlink itself.
    ResolveOptions no_follow;
    no_follow.follow_final = false;
    CHECK_EQ(resolve(t, "/etc/abs", no_follow, r), 0);
    CHECK(r.final_is_symlink);
}

VTEST(resolver_escape) {
    Scratch s;
    REQUIRE(!s.root.empty());
    s.mkdir("/etc");
    s.file("/etc/real");
    // Absolute symlink is interpreted in guest space: it cannot leave the rootfs.
    s.symlink("/../../../../etc/passwd", "/etc/escape");
    // Symlink loop.
    s.symlink("loop-b", "/etc/loop-a");
    s.symlink("loop-a", "/etc/loop-b");
    MountTable t(s.root, false);

    Resolved r;
    int e = resolve(t, "/etc/escape", {}, r);
    // Resolves to <root>/etc/passwd (clamped), which does not exist.
    CHECK_EQ(e, 0);
    CHECK(!r.exists);
    CHECK_EQ(r.host, s.root + "/etc/passwd");

    CHECK_EQ(resolve(t, "/etc/loop-a", {}, r), ELOOP);

    // ".." never climbs above the guest root.
    CHECK_EQ(resolve(t, "/../../etc/real", {}, r), 0);
    CHECK(r.exists);
    CHECK_EQ(r.host, s.root + "/etc/real");
}
