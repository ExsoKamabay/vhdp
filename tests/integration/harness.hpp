// Helpers for integration tests: run phdp as a child, run a guest through
// libvhdp directly, and manage a per-test scratch copy of the fixture rootfs.
#pragma once

#include <string>
#include <vector>

namespace it {

struct RunResult {
    int shell_status = -1; // process exit status decoded to shell convention
    int raw_status = -1;   // waitpid status
    bool timed_out = false;
    std::string out;
    std::string err;
};

// Runs PHDP_BIN with args (argv after the program name). stdin_data is fed to the
// child's stdin (empty closes it). timeout_ms guards against hangs (0 = 20s).
RunResult run_phdp(const std::vector<std::string>& args, const std::string& stdin_data = "",
                   int timeout_ms = 0);

// Runs phdp on a PTY so the guest sees a terminal. keystrokes are written to the
// master after the guest signals readiness (a line is read first). Optionally a
// control byte (e.g. 0x03 for Ctrl-C) is injected.
RunResult run_phdp_pty(const std::vector<std::string>& args, const std::string& initial_input = "",
                       int timeout_ms = 0);

const char* fixture_rootfs();
const char* fixture_min_rootfs();

// Creates a writable throwaway copy of the fixture rootfs; removed on destruction.
class ScratchRootfs {
public:
    ScratchRootfs();
    ~ScratchRootfs();
    const std::string& path() const { return path_; }
    bool ok() const { return !path_.empty(); }

private:
    std::string path_;
};

bool file_contains(const std::string& path, const std::string& needle);
bool have_dynamic_fixture();
std::string trim(const std::string& s);

} // namespace it
