/*
 * Minimal C embedding example: runs `<rootfs> -- <command...>` through libvhdp
 * exactly like `phdp run`, prints lifecycle/diagnostic events to stderr and exits
 * with the guest's shell-style status.
 *
 *   embed_run_c ROOTFS COMMAND [ARG...]
 */
#include <vhdp/vhdp.h>

#include <stdio.h>
#include <string.h>

static void on_event(void* user, const vhdp_event* ev) {
    (void)user;
    if (ev->severity >= VHDP_SEVERITY_WARNING) {
        fprintf(stderr, "embed_run_c: event %s: %s\n", ev->name, ev->message);
    }
}

static int report_failure(const char* what, vhdp_status_t st) {
    char msg[512];
    size_t needed = 0;
    if (vhdp_last_error_message(msg, sizeof(msg), &needed) != VHDP_OK) {
        msg[0] = '\0';
    }
    fprintf(stderr, "embed_run_c: %s failed: %s %s\n", what, vhdp_status_name(st), msg);
    return 125;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s ROOTFS COMMAND [ARG...]\n", argv[0]);
        return 2;
    }
    if (vhdp_abi_version() != VHDP_ABI_VERSION) {
        fprintf(stderr, "embed_run_c: libvhdp ABI %u, compiled against %u\n", vhdp_abi_version(),
                VHDP_ABI_VERSION);
        return 125;
    }

    vhdp_context_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.struct_size = sizeof(opts);
    opts.abi_version = VHDP_ABI_VERSION;
    opts.event_fn = on_event;
    opts.min_severity = VHDP_SEVERITY_WARNING;

    vhdp_context* ctx = NULL;
    vhdp_status_t st = vhdp_context_create(&opts, &ctx);
    if (st != VHDP_OK) {
        return report_failure("vhdp_context_create", st);
    }
    vhdp_config* cfg = NULL;
    st = vhdp_config_create(ctx, &cfg);
    if (st == VHDP_OK) {
        st = vhdp_config_set_rootfs(cfg, argv[1]);
    }
    if (st == VHDP_OK) {
        st = vhdp_config_set_argv(cfg, (size_t)(argc - 2), (const char* const*)(argv + 2));
    }
    if (st != VHDP_OK) {
        int rc = report_failure("config", st);
        vhdp_config_destroy(cfg);
        vhdp_context_destroy(ctx);
        return rc;
    }

    vhdp_session* session = NULL;
    st = vhdp_session_create(ctx, cfg, &session);
    vhdp_config_destroy(cfg); /* the session holds its own copy */
    if (st != VHDP_OK) {
        int rc = report_failure("vhdp_session_create", st);
        vhdp_context_destroy(ctx);
        return rc;
    }

    int exit_status = 125;
    st = vhdp_session_start(session);
    vhdp_exit_info info;
    memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    info.abi_version = VHDP_ABI_VERSION;
    if (st != VHDP_OK) {
        exit_status = report_failure("vhdp_session_start", st);
        if (vhdp_session_wait(session, 0, &info) == VHDP_OK && info.shell_status != 0) {
            exit_status = info.shell_status;
        }
    } else if (vhdp_session_wait(session, -1, &info) == VHDP_OK) {
        exit_status = info.shell_status;
    }

    vhdp_session_destroy(session);
    vhdp_context_destroy(ctx);
    return exit_status;
}
