#include "vtest.hpp"

#include "cli_args.hpp"

#include <variant>

using namespace phdp;

namespace {

Parsed ok(std::vector<std::string> a) {
    auto r = parse_arguments(a);
    auto* p = std::get_if<Parsed>(&r);
    REQUIRE(p != nullptr);
    return *p;
}

bool is_error(std::vector<std::string> a) {
    return std::holds_alternative<ParseError>(parse_arguments(a));
}

} // namespace

VTEST(cli_args) {
    // Explicit run with options and command.
    Parsed p = ok({"run", "--engine", "rootless", "--bind", "/h:/g:rw", "-e", "A=b", "/rootfs",
                   "--", "/bin/sh", "-l"});
    CHECK_EQ(p.command, Command::run);
    CHECK(!p.implicit_run);
    CHECK_EQ(p.run.engine, "rootless");
    REQUIRE_EQ(p.run.binds.size(), 1u);
    CHECK(p.run.binds[0].read_write);
    CHECK_EQ(p.run.rootfs, "/rootfs");
    REQUIRE_EQ(p.run.command.size(), 2u);
    CHECK_EQ(p.run.command[0], "/bin/sh");

    // Implicit run (positional alias).
    Parsed q = ok({"/rootfs", "--", "/usr/bin/env"});
    CHECK_EQ(q.command, Command::run);
    CHECK(q.implicit_run);
    CHECK_EQ(q.run.rootfs, "/rootfs");

    // Inline --opt=value and --timeout parsing.
    Parsed t = ok({"run", "--timeout=1.5", "--uid=0", "/r"});
    CHECK_EQ(t.run.timeout_ms.value(), 1500u);
    CHECK_EQ(t.run.uid.value(), 0u);

    // Reports.
    CHECK_EQ(ok({"doctor", "--json"}).command, Command::doctor);
    CHECK(ok({"doctor", "--json"}).json);
    CHECK(ok({"doctor", "--no-active"}).no_active);
    CHECK_EQ(ok({"inspect", "/r"}).inspect_path, "/r");
    CHECK_EQ(ok({"capabilities"}).command, Command::capabilities);
    CHECK_EQ(ok({"--help"}).command, Command::help);
    CHECK_EQ(ok({"--version"}).command, Command::version);
}

VTEST(cli_args_disambiguation) {
    // Subcommand names in first position are subcommands.
    CHECK_EQ(ok({"doctor"}).command, Command::doctor);
    CHECK_EQ(ok({"version"}).command, Command::version);
    // A rootfs literally named like a subcommand must go through `run` or a path.
    Parsed p = ok({"run", "doctor", "--", "/bin/sh"});
    CHECK_EQ(p.command, Command::run);
    CHECK_EQ(p.run.rootfs, "doctor");
    Parsed q = ok({"./doctor"});
    CHECK_EQ(q.command, Command::run);
    CHECK_EQ(q.run.rootfs, "./doctor");

    // Errors.
    CHECK(is_error({}));
    CHECK(is_error({"run"}));                        // no rootfs
    CHECK(is_error({"run", "--unknown", "/r"}));     // unknown option
    CHECK(is_error({"run", "/r", "extra"}));         // stray arg after rootfs
    CHECK(is_error({"run", "/r", "--engine", "x"})); // option after rootfs
    CHECK(is_error({"run", "--engine", "bogus", "/r"}));
    CHECK(is_error({"run", "--bind", "onlyone", "/r"}));
    CHECK(is_error({"run", "--timeout", "0", "/r"}));
    CHECK(is_error({"inspect"}));
    CHECK(is_error({"run", "/r", "--"}));      // -- with no command
    CHECK(is_error({"--engine", "rootless"})); // implicit run without rootfs
}
