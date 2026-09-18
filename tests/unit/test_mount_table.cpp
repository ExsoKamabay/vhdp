#include "vtest.hpp"

#include "vfs/mount_table.hpp"

using namespace vhdp::vfs;

VTEST(mount_table_map) {
    MountTable t("/host/root", false);
    t.add(Mount{"/dev/null", "/host/dev/null", false, MountKind::device, "vdr:dev-minimal"});
    t.add(Mount{"/mnt/shared", "/host/share", true, MountKind::bind, "host-bind"});

    CHECK_EQ(t.to_host("/etc/passwd").host, "/host/root/etc/passwd");
    CHECK_EQ(t.to_host("/").host, "/host/root");
    CHECK_EQ(t.to_host("/dev/null").host, "/host/dev/null");
    CHECK_EQ(t.to_host("/mnt/shared/file").host, "/host/share/file");

    // Longest-prefix wins: /mnt/shared beats the rootfs.
    auto m = t.mount_for_guest("/mnt/shared/x");
    REQUIRE(m != nullptr);
    CHECK_EQ(m->source, "host-bind");
    CHECK(m->read_only);

    // Reverse mapping.
    auto g = t.to_guest("/host/share/file");
    REQUIRE(g.has_value());
    CHECK_EQ(*g, "/mnt/shared/file");
    CHECK_EQ(*t.to_guest("/host/root/etc"), "/etc");
    CHECK(!t.to_guest("/somewhere/else").has_value());
}
