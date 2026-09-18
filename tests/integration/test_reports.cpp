#include "harness.hpp"
#include "vtest.hpp"

#include "common/json.hpp"

using namespace it;

VTEST(cli_doctor_json) {
    RunResult r = run_phdp({"doctor", "--json"});
    CHECK(vhdp::json_validate(trim(r.out)));
    CHECK_NE(r.out.find("\"detected_profile\""), std::string::npos);
    CHECK_NE(r.out.find("\"ptrace.child\""), std::string::npos);
    // Human form works too.
    RunResult h = run_phdp({"doctor"});
    CHECK_NE(h.out.find("Engines:"), std::string::npos);
}

VTEST(cli_capabilities_json) {
    RunResult r = run_phdp({"capabilities", "--json"});
    CHECK(vhdp::json_validate(trim(r.out)));
    CHECK_NE(r.out.find("\"features\""), std::string::npos);
    CHECK_NE(r.out.find("T-CLI-RUN-EXIT37"), std::string::npos);
    CHECK_NE(r.out.find("compatibility isolation"), std::string::npos);
    CHECK_NE(r.out.find("backend-required"), std::string::npos);
}

VTEST(cli_inspect_json) {
    RunResult r = run_phdp({"inspect", "--json", fixture_rootfs()});
    CHECK(vhdp::json_validate(trim(r.out)));
    CHECK_NE(r.out.find("\"os_release\""), std::string::npos);
    CHECK_NE(r.out.find("vhdpfixture"), std::string::npos);
    CHECK_NE(r.out.find("\"shells\""), std::string::npos);

    // Missing rootfs -> error status, exit 1.
    RunResult bad = run_phdp({"inspect", "/no/such/rootfs/vhdp"});
    CHECK_EQ(bad.shell_status, 1);
}

VTEST(cli_json_events) {
    // JSON events go to stderr; guest stdout stays clean.
    RunResult r =
        run_phdp({"run", "--json", "--verbose", fixture_rootfs(), "--", "/bin/echo", "GUESTOUT"});
    CHECK_EQ(r.shell_status, 0);
    CHECK_EQ(trim(r.out), "GUESTOUT");
    CHECK_EQ(r.out.find('{'), std::string::npos); // no JSON leaked into stdout
    // Every stderr line is a JSON object.
    std::size_t start = 0;
    int lines = 0;
    while (start < r.err.size()) {
        std::size_t nl = r.err.find('\n', start);
        std::string line =
            r.err.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!trim(line).empty()) {
            CHECK(vhdp::json_validate(trim(line)));
            ++lines;
        }
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
    CHECK(lines > 0);
}
