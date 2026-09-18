/*
 * vhdp-fixture: project-owned multi-call guest program for integration tests.
 *
 * Built statically (and as dynamic / missing-loader variants) and installed into a
 * generated fixture rootfs. Dispatch uses basename(argv[0]) or, when invoked as
 * "vhdp-fixture", argv[1]. Output formats are stable because tests parse them.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char* errname(int e) {
    switch (e) {
        case 0:
            return "ok";
        case EPERM:
            return "EPERM";
        case ENOENT:
            return "ENOENT";
        case ESRCH:
            return "ESRCH";
        case EACCES:
            return "EACCES";
        case EEXIST:
            return "EEXIST";
        case EXDEV:
            return "EXDEV";
        case ENOTDIR:
            return "ENOTDIR";
        case EISDIR:
            return "EISDIR";
        case EINVAL:
            return "EINVAL";
        case EROFS:
            return "EROFS";
        case ELOOP:
            return "ELOOP";
        case ENOSYS:
            return "ENOSYS";
        case ENOEXEC:
            return "ENOEXEC";
        case EBADF:
            return "EBADF";
        case ENAMETOOLONG:
            return "ENAMETOOLONG";
        case ENOTEMPTY:
            return "ENOTEMPTY";
        case EFAULT:
            return "EFAULT";
        case EOPNOTSUPP:
            return "EOPNOTSUPP";
        case ECONNREFUSED:
            return "ECONNREFUSED";
        default:
            return "E_OTHER";
    }
}

static int report(int rc) {
    printf("%s\n", rc == 0 ? "ok" : errname(errno));
    return 0;
}

static int write_file(const char* path, const char* text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return -1;
    }
    size_t len = strlen(text);
    ssize_t n = write(fd, text, len);
    int saved = errno;
    close(fd);
    errno = saved;
    return n == (ssize_t)len ? 0 : -1;
}

static int cmp_str(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static int applet_ls(const char* dir) {
    DIR* d = opendir(dir);
    if (d == NULL) {
        printf("%s\n", errname(errno));
        return 1;
    }
    char* names[1024];
    size_t n = 0;
    struct dirent* e;
    while ((e = readdir(d)) != NULL && n < 1024) {
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
            names[n++] = strdup(e->d_name);
        }
    }
    closedir(d);
    qsort(names, n, sizeof(names[0]), cmp_str);
    for (size_t i = 0; i < n; ++i) {
        printf("%s\n", names[i]);
        free(names[i]);
    }
    return 0;
}

static int applet_cat(const char* path) {
    int fd = path == NULL ? 0 : open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "cat: %s\n", errname(errno));
        return 1;
    }
    char buf[65536];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(1, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return 1;
            }
            off += w;
        }
    }
    return 0;
}

static int applet_sleep(const char* arg) {
    double s = arg != NULL ? atof(arg) : 1.0;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
    return 0;
}

static int applet_stat(const char* path, int follow) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) {
        printf("%s\n", errname(errno));
        return 0;
    }
    const char* type = S_ISDIR(st.st_mode)   ? "dir"
                       : S_ISLNK(st.st_mode) ? "link"
                       : S_ISREG(st.st_mode) ? "file"
                       : S_ISCHR(st.st_mode) ? "chr"
                                             : "other";
    printf("%s uid=%u gid=%u size=%lld\n", type, (unsigned)st.st_uid, (unsigned)st.st_gid,
           (long long)st.st_size);
    return 0;
}

static void* thread_main(void* arg) {
    struct stat st;
    (void)arg;
    for (int i = 0; i < 50; ++i) {
        if (stat("/etc/hello.txt", &st) != 0) {
            return (void*)1;
        }
    }
    return NULL;
}

static int applet_try(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "try: missing operation\n");
        return 2;
    }
    const char* op = argv[0];
    const char* a = argc > 1 ? argv[1] : "";
    const char* b = argc > 2 ? argv[2] : "";
    if (strcmp(op, "open-r") == 0) {
        int fd = open(a, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
        }
        return report(fd >= 0 ? 0 : -1);
    }
    if (strcmp(op, "open-w") == 0) {
        int fd = open(a, O_WRONLY | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
        }
        return report(fd >= 0 ? 0 : -1);
    }
    if (strcmp(op, "create") == 0) {
        return report(write_file(a, b));
    }
    if (strcmp(op, "mkdir") == 0) {
        return report(mkdir(a, 0755));
    }
    if (strcmp(op, "rmdir") == 0) {
        return report(rmdir(a));
    }
    if (strcmp(op, "unlink") == 0) {
        return report(unlink(a));
    }
    if (strcmp(op, "rename") == 0) {
        return report(rename(a, b));
    }
    if (strcmp(op, "link") == 0) {
        return report(link(a, b));
    }
    if (strcmp(op, "symlink") == 0) {
        return report(symlink(a, b));
    }
    if (strcmp(op, "chdir") == 0) {
        return report(chdir(a));
    }
    if (strcmp(op, "chmod") == 0) {
        return report(chmod(a, 0600));
    }
    if (strcmp(op, "access-w") == 0) {
        return report(access(a, W_OK));
    }
    if (strcmp(op, "truncate") == 0) {
        return report(truncate(a, 0));
    }
    if (strcmp(op, "openat") == 0) {
        /* openat relative to a directory descriptor */
        int dfd = open(a, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dfd < 0) {
            return report(-1);
        }
        int fd = openat(dfd, b, O_RDONLY | O_CLOEXEC);
        int saved = errno;
        if (fd >= 0) {
            char buf[256];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            buf[n > 0 ? n : 0] = '\0';
            printf("ok %s", buf);
            close(fd);
            close(dfd);
            return 0;
        }
        close(dfd);
        errno = saved;
        return report(-1);
    }
    if (strcmp(op, "fd-openat") == 0) {
        /* openat relative to an already-open (possibly inherited) descriptor number */
        int dfd = atoi(a);
        if (fcntl(dfd, F_GETFD) < 0) {
            printf("closed\n");
            return 0;
        }
        int fd = openat(dfd, b, O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            close(fd);
        }
        return report(fd >= 0 ? 0 : -1);
    }
    if (strcmp(op, "socket-inet") == 0) {
        int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (s >= 0) {
            close(s);
        }
        return report(s >= 0 ? 0 : -1);
    }
    if (strcmp(op, "socket-unix") == 0) {
        int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (s >= 0) {
            close(s);
        }
        return report(s >= 0 ? 0 : -1);
    }
    if (strcmp(op, "syscall") == 0) {
        long r = syscall(atol(a), 0, 0, 0, 0, 0, 0);
        return report(r < 0 ? -1 : 0);
    }
    if (strcmp(op, "mount") == 0) {
        /* Always denied by the rootless policy (EPERM), architecture-independent. */
        errno = 0;
        long r = syscall(SYS_mount, (long)"none", (long)"/tmp", (long)"tmpfs", 0L, 0L, 0L);
        return report(r < 0 ? -1 : 0);
    }
    if (strcmp(op, "unshare") == 0) {
        errno = 0;
        long r = syscall(SYS_unshare, 0x10000000L /* CLONE_NEWNS */, 0, 0, 0, 0, 0);
        return report(r < 0 ? -1 : 0);
    }
    if (strcmp(op, "chroot") == 0) {
        errno = 0;
        long r = syscall(SYS_chroot, (long)(a[0] ? a : "/"), 0, 0, 0, 0, 0);
        return report(r < 0 ? -1 : 0);
    }
    if (strcmp(op, "kill0") == 0) {
        return report(kill((pid_t)atol(a), 0));
    }
    if (strcmp(op, "exec") == 0) {
        char* args[] = {(char*)a, NULL};
        execv(a, args);
        printf("%s\n", errname(errno));
        return 0;
    }
    fprintf(stderr, "try: unknown operation %s\n", op);
    return 2;
}

static int applet_unix_roundtrip(const char* path) {
    int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(path);
    if (srv < 0 || bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(srv, 1) != 0) {
        printf("bind %s\n", errname(errno));
        return 1;
    }
    pid_t child = fork();
    if (child == 0) {
        int c = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (c < 0 || connect(c, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            _exit(3);
        }
        (void)write(c, "ping", 4);
        _exit(0);
    }
    int conn = accept(srv, NULL, NULL);
    char buf[8] = {0};
    ssize_t n = conn >= 0 ? read(conn, buf, 4) : -1;
    int status = 0;
    waitpid(child, &status, 0);
    struct stat st;
    int exists = stat(path, &st) == 0 && S_ISSOCK(st.st_mode);
    unlink(path);
    printf("unix %s %s\n", n == 4 && strcmp(buf, "ping") == 0 ? "ok" : "fail",
           exists ? "socket-visible" : "socket-missing");
    return 0;
}

static int applet_spawn_tree(int width) {
    for (int i = 0; i < width; ++i) {
        pid_t c = fork();
        if (c == 0) {
            pid_t g = fork();
            if (g == 0) {
                for (;;) {
                    pause();
                }
            }
            printf("%d\n", (int)g);
            fflush(stdout);
            for (;;) {
                pause();
            }
        }
        printf("%d\n", (int)c);
        fflush(stdout);
    }
    printf("tree-ready\n");
    fflush(stdout);
    for (;;) {
        pause();
    }
}

static int applet_fanout(int n) {
    for (int i = 0; i < n; ++i) {
        pid_t c = fork();
        if (c == 0) {
            struct stat st;
            _exit(stat("/etc/hello.txt", &st) == 0 ? (i % 200) : 250);
        }
        if (c < 0) {
            printf("fork failed %s\n", errname(errno));
            return 1;
        }
    }
    int bad = 0;
    for (int i = 0; i < n; ++i) {
        int status = 0;
        if (wait(&status) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) == 250) {
            ++bad;
        }
    }
    pthread_t th[8];
    for (int i = 0; i < 8; ++i) {
        pthread_create(&th[i], NULL, thread_main, NULL);
    }
    for (int i = 0; i < 8; ++i) {
        void* r;
        pthread_join(th[i], &r);
        if (r != NULL) {
            ++bad;
        }
    }
    printf("fanout %s %d\n", bad == 0 ? "ok" : "fail", n);
    return bad == 0 ? 0 : 1;
}

static int check_fds(void) {
    for (int fd = 0; fd < 1024; ++fd) {
        if (fcntl(fd, F_GETFD) >= 0) {
            printf("%d\n", fd);
        }
    }
    return 0;
}

/* Minimal shell: `sh -c "cmd args"` or an interactive read-eval loop.
 * Supports whitespace-separated words, double quotes, `exit [N]` and `cd DIR`. */
static int sh_split(char* line, char** argv, int max) {
    int argc = 0;
    char* p = line;
    while (*p != '\0' && argc < max - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        char* start;
        if (*p == '"') {
            start = ++p;
            while (*p != '\0' && *p != '"') {
                ++p;
            }
        } else {
            start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                ++p;
            }
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
        argv[argc++] = start;
    }
    argv[argc] = NULL;
    return argc;
}

static int sh_run(char* line, int* last) {
    char* argv[64];
    int argc = sh_split(line, argv, 64);
    if (argc == 0) {
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        exit(argc > 1 ? atoi(argv[1]) : *last);
    }
    if (strcmp(argv[0], "cd") == 0) {
        *last = chdir(argc > 1 ? argv[1] : "/") == 0 ? 0 : 1;
        return 0;
    }
    pid_t c = fork();
    if (c == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "sh: %s: %s\n", argv[0], errname(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }
    int status = 0;
    while (waitpid(c, &status, 0) < 0 && errno == EINTR) {
    }
    *last = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return 0;
}

static int applet_sh(int argc, char** argv) {
    int last = 0;
    if (argc >= 2 && strcmp(argv[0], "-c") == 0) {
        char* line = strdup(argv[1]);
        sh_run(line, &last);
        free(line);
        return last;
    }
    int interactive = isatty(0);
    char line[4096];
    for (;;) {
        if (interactive) {
            fputs("fixture$ ", stdout);
            fflush(stdout);
        }
        if (fgets(line, sizeof(line), stdin) == NULL) {
            return last;
        }
        sh_run(line, &last);
        fflush(stdout);
    }
}

int main(int argc, char** argv) {
    const char* name = strrchr(argv[0], '/');
    name = name != NULL ? name + 1 : argv[0];
    int shift = 1;
    if (strcmp(name, "vhdp-fixture") == 0 || strcmp(name, "vhdp-fixture-dyn") == 0 ||
        strcmp(name, "no-loader") == 0) {
        if (argc < 2) {
            fprintf(stderr, "usage: vhdp-fixture APPLET [ARGS]\n");
            return 2;
        }
        name = argv[1];
        shift = 2;
    }
    int ac = argc - shift;
    char** av = argv + shift;
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (strcmp(name, "exit") == 0) {
        return ac > 0 ? atoi(av[0]) : 0;
    }
    if (strcmp(name, "echo") == 0) {
        for (int i = 0; i < ac; ++i) {
            printf("%s%s", i ? " " : "", av[i]);
        }
        printf("\n");
        return 0;
    }
    if (strcmp(name, "printenv") == 0) {
        if (ac > 0) {
            const char* v = getenv(av[0]);
            if (v == NULL) {
                return 1;
            }
            printf("%s\n", v);
            return 0;
        }
        for (char** e = environ; *e != NULL; ++e) {
            printf("%s\n", *e);
        }
        return 0;
    }
    if (strcmp(name, "pwd") == 0) {
        char buf[PATH_MAX];
        if (getcwd(buf, sizeof(buf)) == NULL) {
            printf("%s\n", errname(errno));
            return 1;
        }
        printf("%s\n", buf);
        return 0;
    }
    if (strcmp(name, "cat") == 0) {
        return applet_cat(ac > 0 ? av[0] : NULL);
    }
    if (strcmp(name, "ls") == 0) {
        return applet_ls(ac > 0 ? av[0] : ".");
    }
    if (strcmp(name, "stat") == 0 && ac > 0) {
        return applet_stat(av[0], 1);
    }
    if (strcmp(name, "lstat") == 0 && ac > 0) {
        return applet_stat(av[0], 0);
    }
    if (strcmp(name, "readlink") == 0 && ac > 0) {
        char buf[PATH_MAX];
        ssize_t n = readlink(av[0], buf, sizeof(buf) - 1);
        if (n < 0) {
            printf("%s\n", errname(errno));
            return 0;
        }
        buf[n] = '\0';
        printf("%s\n", buf);
        return 0;
    }
    if (strcmp(name, "chdir-pwd") == 0 && ac > 0) {
        char buf[PATH_MAX];
        if (chdir(av[0]) != 0) {
            printf("%s\n", errname(errno));
            return 0;
        }
        printf("%s\n", getcwd(buf, sizeof(buf)) != NULL ? buf : errname(errno));
        return 0;
    }
    if (strcmp(name, "cwd-deleted") == 0 && ac > 0) {
        char buf[PATH_MAX];
        if (mkdir(av[0], 0755) != 0 || chdir(av[0]) != 0 || rmdir(av[0]) != 0) {
            printf("setup %s\n", errname(errno));
            return 1;
        }
        printf("getcwd %s\n", getcwd(buf, sizeof(buf)) != NULL ? "ok" : errname(errno));
        int fd = open("x", O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
        printf("create %s\n", fd >= 0 ? "ok" : errname(errno));
        return 0;
    }
    if (strcmp(name, "sleep") == 0) {
        return applet_sleep(ac > 0 ? av[0] : NULL);
    }
    if (strcmp(name, "winsize") == 0) {
        struct winsize ws;
        if (ioctl(0, TIOCGWINSZ, &ws) != 0) {
            printf("winsize %s\n", errname(errno));
            return 1;
        }
        printf("winsize %u %u\n", (unsigned)ws.ws_row, (unsigned)ws.ws_col);
        return 0;
    }
    if (strcmp(name, "wait-winch") == 0) {
        /* Prints the size, waits for SIGWINCH, prints the new size. */
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGWINCH);
        sigprocmask(SIG_BLOCK, &set, NULL);
        struct winsize ws;
        ioctl(0, TIOCGWINSZ, &ws);
        printf("size %u %u\n", (unsigned)ws.ws_row, (unsigned)ws.ws_col);
        fflush(stdout);
        int sig;
        sigwait(&set, &sig);
        ioctl(0, TIOCGWINSZ, &ws);
        printf("resized %u %u\n", (unsigned)ws.ws_row, (unsigned)ws.ws_col);
        return 0;
    }
    if (strcmp(name, "ready-sleep") == 0) {
        printf("ready\n");
        fflush(stdout);
        return applet_sleep(ac > 0 ? av[0] : "30");
    }
    if (strcmp(name, "uid") == 0) {
        uid_t r, e, s;
        gid_t gr, ge, gs;
        getresuid(&r, &e, &s);
        getresgid(&gr, &ge, &gs);
        printf("uid=%u euid=%u gid=%u egid=%u resuid=%u,%u,%u resgid=%u,%u,%u\n",
               (unsigned)getuid(), (unsigned)geteuid(), (unsigned)getgid(), (unsigned)getegid(),
               (unsigned)r, (unsigned)e, (unsigned)s, (unsigned)gr, (unsigned)ge, (unsigned)gs);
        return 0;
    }
    if (strcmp(name, "try") == 0) {
        return applet_try(ac, av);
    }
    if (strcmp(name, "unix-roundtrip") == 0 && ac > 0) {
        return applet_unix_roundtrip(av[0]);
    }
    if (strcmp(name, "spawn-tree") == 0) {
        return applet_spawn_tree(ac > 0 ? atoi(av[0]) : 3);
    }
    if (strcmp(name, "fanout") == 0) {
        return applet_fanout(ac > 0 ? atoi(av[0]) : 32);
    }
    if (strcmp(name, "check-fds") == 0) {
        return check_fds();
    }
    if (strcmp(name, "self-exe") == 0) {
        char buf[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n < 0) {
            printf("%s\n", errname(errno));
            return 0;
        }
        buf[n] = '\0';
        printf("%s\n", buf);
        return 0;
    }
    if (strcmp(name, "write") == 0 && ac > 1) {
        return report(write_file(av[0], av[1]));
    }
    if (strcmp(name, "sh") == 0) {
        return applet_sh(ac, av);
    }
    if (strcmp(name, "bench-stat") == 0) {
        /* Timing loop used by benchmarks: N stat() calls, prints nanoseconds. */
        long n = ac > 0 ? atol(av[0]) : 100000;
        struct timespec t0, t1;
        struct stat st;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < n; ++i) {
            (void)stat("/etc/hello.txt", &st);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("%lld\n",
               (long long)((t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec)));
        return 0;
    }
    if (strcmp(name, "bench-getpid") == 0) {
        long n = ac > 0 ? atol(av[0]) : 100000;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < n; ++i) {
            (void)syscall(SYS_getppid);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("%lld\n",
               (long long)((t1.tv_sec - t0.tv_sec) * 1000000000LL + (t1.tv_nsec - t0.tv_nsec)));
        return 0;
    }
    fprintf(stderr, "vhdp-fixture: unknown applet '%s'\n", name);
    return 2;
}
