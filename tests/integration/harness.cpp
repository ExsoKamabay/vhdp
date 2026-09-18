#include "harness.hpp"

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

extern char** environ;

namespace it {

namespace {

int decode_status(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
}

// Drains fds into their strings while waiting for the child; kills it on timeout.
RunResult pump_and_wait(pid_t pid, int out_fd, int err_fd, int in_fd, const std::string& stdin_data,
                        int timeout_ms) {
    RunResult r;
    if (timeout_ms <= 0) {
        timeout_ms = 20000;
    }
    std::size_t written = 0;
    if (in_fd >= 0 && stdin_data.empty()) {
        ::close(in_fd);
        in_fd = -1;
    }
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bool reaped = false;
    int status = 0;
    while (true) {
        pollfd fds[3];
        int n = 0;
        int out_idx = -1;
        int err_idx = -1;
        int in_idx = -1;
        if (out_fd >= 0) {
            out_idx = n;
            fds[n++] = {out_fd, POLLIN, 0};
        }
        if (err_fd >= 0) {
            err_idx = n;
            fds[n++] = {err_fd, POLLIN, 0};
        }
        if (in_fd >= 0) {
            in_idx = n;
            fds[n++] = {in_fd, POLLOUT, 0};
        }
        int pr = n > 0 ? ::poll(fds, static_cast<nfds_t>(n), 100) : 0;
        if (pr < 0 && errno != EINTR) {
            break;
        }
        char buf[8192];
        if (out_idx >= 0 && (fds[out_idx].revents & (POLLIN | POLLHUP)) != 0) {
            ssize_t k = ::read(out_fd, buf, sizeof(buf));
            if (k > 0) {
                r.out.append(buf, static_cast<std::size_t>(k));
            } else if (k == 0) {
                ::close(out_fd);
                out_fd = -1;
            }
        }
        if (err_idx >= 0 && (fds[err_idx].revents & (POLLIN | POLLHUP)) != 0) {
            ssize_t k = ::read(err_fd, buf, sizeof(buf));
            if (k > 0) {
                r.err.append(buf, static_cast<std::size_t>(k));
            } else if (k == 0) {
                ::close(err_fd);
                err_fd = -1;
            }
        }
        if (in_idx >= 0 && (fds[in_idx].revents & POLLOUT) != 0) {
            ssize_t k = ::write(in_fd, stdin_data.data() + written, stdin_data.size() - written);
            if (k > 0) {
                written += static_cast<std::size_t>(k);
            }
            if (written >= stdin_data.size() || k < 0) {
                ::close(in_fd);
                in_fd = -1;
            }
        }
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            reaped = true;
            // Drain any remaining buffered output.
            for (int fd : {out_fd, err_fd}) {
                if (fd < 0) {
                    continue;
                }
                ::fcntl(fd, F_SETFL, O_NONBLOCK);
                ssize_t k;
                while ((k = ::read(fd, buf, sizeof(buf))) > 0) {
                    (fd == out_fd ? r.out : r.err).append(buf, static_cast<std::size_t>(k));
                }
            }
            break;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed > timeout_ms) {
            ::kill(pid, SIGKILL);
            r.timed_out = true;
            ::waitpid(pid, &status, 0);
            reaped = true;
            break;
        }
    }
    if (out_fd >= 0) {
        ::close(out_fd);
    }
    if (err_fd >= 0) {
        ::close(err_fd);
    }
    if (in_fd >= 0) {
        ::close(in_fd);
    }
    if (!reaped) {
        ::waitpid(pid, &status, 0);
    }
    r.raw_status = status;
    r.shell_status = decode_status(status);
    return r;
}

} // namespace

RunResult run_phdp(const std::vector<std::string>& args, const std::string& stdin_data,
                   int timeout_ms) {
    int out_pipe[2];
    int err_pipe[2];
    int in_pipe[2];
    RunResult fail;
    if (::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0 || ::pipe(in_pipe) != 0) {
        return fail;
    }
    std::vector<std::string> argv_str = {PHDP_BIN};
    for (const auto& a : args) {
        argv_str.push_back(a);
    }
    std::vector<char*> argv;
    for (auto& a : argv_str) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, in_pipe[0], 0);
    posix_spawn_file_actions_adddup2(&fa, out_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&fa, err_pipe[1], 2);
    posix_spawn_file_actions_addclose(&fa, in_pipe[1]);
    posix_spawn_file_actions_addclose(&fa, out_pipe[0]);
    posix_spawn_file_actions_addclose(&fa, err_pipe[0]);

    pid_t pid = -1;
    int rc = posix_spawn(&pid, PHDP_BIN, &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);
    if (rc != 0) {
        ::close(in_pipe[1]);
        ::close(out_pipe[0]);
        ::close(err_pipe[0]);
        return fail;
    }
    return pump_and_wait(pid, out_pipe[0], err_pipe[0], in_pipe[1], stdin_data, timeout_ms);
}

RunResult run_phdp_pty(const std::vector<std::string>& args, const std::string& initial_input,
                       int timeout_ms) {
    int master = -1;
    RunResult fail;
    winsize ws{};
    ws.ws_row = 24;
    ws.ws_col = 80;
    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        return fail;
    }
    if (pid == 0) {
        std::vector<std::string> argv_str = {PHDP_BIN};
        for (const auto& a : args) {
            argv_str.push_back(a);
        }
        std::vector<char*> argv;
        for (auto& a : argv_str) {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);
        execv(PHDP_BIN, argv.data());
        _exit(127);
    }
    if (!initial_input.empty()) {
        // Wait briefly for the guest to be ready, then send input.
        struct timespec ts{0, 150 * 1000 * 1000};
        nanosleep(&ts, nullptr);
        ssize_t wn = ::write(master, initial_input.data(), initial_input.size());
        (void)wn;
    }
    return pump_and_wait(pid, master, -1, -1, "", timeout_ms);
}

const char* fixture_rootfs() {
    return FIXTURE_ROOTFS;
}
const char* fixture_min_rootfs() {
    return FIXTURE_MIN_ROOTFS;
}

ScratchRootfs::ScratchRootfs() {
    const char* base = std::getenv("TMPDIR");
    std::string tmpl = std::string(base != nullptr ? base : "/tmp") + "/vhdp-scratch-XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* d = ::mkdtemp(buf.data());
    if (d == nullptr) {
        return;
    }
    std::string dst = d;
    // cp -a preserves symlinks, modes and directory structure.
    std::string cmd = "cp -a '" + std::string(FIXTURE_ROOTFS) + "/.' '" + dst + "/'";
    if (std::system(cmd.c_str()) == 0) {
        path_ = dst;
    } else {
        std::string rm = "rm -rf '" + dst + "'";
        int rc = std::system(rm.c_str());
        (void)rc;
    }
}

ScratchRootfs::~ScratchRootfs() {
    if (!path_.empty()) {
        std::string rm = "rm -rf '" + path_ + "'";
        int rc = std::system(rm.c_str());
        (void)rc;
    }
}

bool file_contains(const std::string& path, const std::string& needle) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str().find(needle) != std::string::npos;
}

bool have_dynamic_fixture() {
    struct stat st{};
    return ::stat((std::string(FIXTURE_ROOTFS) + "/.no-dynamic").c_str(), &st) != 0 &&
           ::stat((std::string(FIXTURE_ROOTFS) + "/bin/dyn-echo").c_str(), &st) == 0;
}

std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

} // namespace it
