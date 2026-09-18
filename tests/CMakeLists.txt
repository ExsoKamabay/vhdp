# ---- test framework ----
add_library(vtest STATIC framework/vtest_main.cpp)
target_include_directories(vtest PUBLIC framework)
target_link_libraries(vtest PRIVATE vhdp_flags)

# Registers each self-registered case in a vtest binary as an individual CTest.
# CASES is a ;-list of "case_name=TEST-ID" pairs.
function(vhdp_register_cases binary)
  cmake_parse_arguments(A "" "" "CASES;LABELS;FIXTURES" ${ARGN})
  foreach(pair IN LISTS A_CASES)
    string(REPLACE "=" ";" kv "${pair}")
    list(GET kv 0 case)
    list(GET kv 1 id)
    vhdp_add_test(${id} COMMAND $<TARGET_FILE:${binary}> ${case} LABELS "${A_LABELS}"
                  FIXTURES_REQUIRED "${A_FIXTURES}")
  endforeach()
endfunction()

# ---- unit tests (white-box, link the static lib + internal headers) ----
add_executable(unit_tests
  unit/test_json.cpp
  unit/test_strings.cpp
  unit/test_elf.cpp
  unit/test_syscall_table.cpp
  unit/test_guest_path.cpp
  unit/test_mount_table.cpp
  unit/test_resolver.cpp
  unit/test_config.cpp
  unit/test_lifecycle.cpp
  unit/test_engine_select.cpp
  unit/test_seccomp_filter.cpp
  unit/test_raw_syscall.cpp
  unit/test_cli_args.cpp
  unit/test_capi.cpp
  unit/test_cpp_wrapper.cpp)
target_link_libraries(unit_tests PRIVATE vtest vhdp_internal phdp_cli vhdp_flags vhdp_warnings)

vhdp_register_cases(unit_tests LABELS unit CASES
  json_roundtrip=T-UNIT-JSON-ROUNDTRIP
  json_escape=T-UNIT-JSON-ESCAPE
  json_validate=T-UNIT-JSON-VALIDATE
  strings_parse=T-UNIT-STRINGS
  elf_static=T-UNIT-ELF-STATIC
  elf_dynamic=T-UNIT-ELF-DYNAMIC
  elf_malformed=T-UNIT-ELF-MALFORMED
  elf_foreign=T-UNIT-ELF-FOREIGN
  syscall_table_complete=T-UNIT-SYSCALL-TABLE
  syscall_table_lookup=T-UNIT-SYSCALL-LOOKUP
  guest_path_components=T-UNIT-GUESTPATH
  guest_path_normalize=T-UNIT-GUESTPATH-NORM
  mount_table_map=T-UNIT-MOUNT-TABLE
  resolver_basic=T-UNIT-RESOLVER
  resolver_symlink=T-UNIT-RESOLVER-SYMLINK
  resolver_escape=T-UNIT-RESOLVER-ESCAPE
  config_validate=T-UNIT-CONFIG
  config_env=T-UNIT-CONFIG-ENV
  lifecycle_transitions=T-UNIT-LIFECYCLE
  engine_select=T-UNIT-ENGINE-SELECT
  seccomp_filter=T-UNIT-SECCOMP-FILTER
  raw_syscall_equiv=T-UNIT-RAW-SYSCALL
  raw_syscall_regs=T-DIFF-RAW-SYSCALL-REGS
  cli_args=T-UNIT-CLI-ARGS
  cli_args_disambiguation=T-UNIT-CLI-DISAMBIGUATION
  capi_lifecycle=T-UNIT-CAPI
  capi_abi_guard=T-UNIT-CAPI-ABI
  cpp_wrapper=T-UNIT-CPP-WRAPPER)

# ---- fixtures: project-owned guest programs and rootfs ----
# Fixture binaries are plain executables (no sanitizers, no project warnings).
add_executable(vhdp_fixture_static fixtures/vhdp_fixture.c)
set_target_properties(vhdp_fixture_static PROPERTIES OUTPUT_NAME vhdp-fixture-static)
target_link_options(vhdp_fixture_static PRIVATE -static)
find_package(Threads REQUIRED)
target_link_libraries(vhdp_fixture_static PRIVATE Threads::Threads)

add_executable(vhdp_fixture_dyn fixtures/vhdp_fixture.c)
set_target_properties(vhdp_fixture_dyn PROPERTIES OUTPUT_NAME vhdp-fixture-dyn)
target_link_libraries(vhdp_fixture_dyn PRIVATE Threads::Threads)

add_executable(gen_foreign_elf fixtures/gen_foreign_elf.c)

set(FIXTURE_ROOTFS "${CMAKE_CURRENT_BINARY_DIR}/rootfs")
set(FIXTURE_MIN_ROOTFS "${CMAKE_CURRENT_BINARY_DIR}/rootfs-min")
find_program(HOST_LDD ldd)
if(NOT VHDP_LLVM_READELF)
  find_program(VHDP_LLVM_READELF NAMES llvm-readelf readelf)
endif()

# Assemble the rootfs, populate the dynamic loader, add the foreign ELF.
add_custom_command(
  OUTPUT "${FIXTURE_ROOTFS}/.stamp"
  DEPENDS vhdp_fixture_static vhdp_fixture_dyn gen_foreign_elf
          "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/make_rootfs.cmake"
          "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/populate_dynamic.cmake"
  COMMAND ${CMAKE_COMMAND}
          -DROOTFS=${FIXTURE_ROOTFS}
          -DSTATIC_BIN=$<TARGET_FILE:vhdp_fixture_static>
          -DDYN_BIN=$<TARGET_FILE:vhdp_fixture_dyn>
          -DNOLOADER_BIN=
          -P "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/make_rootfs.cmake"
  COMMAND ${CMAKE_COMMAND}
          -DROOTFS=${FIXTURE_ROOTFS} -DBIN=$<TARGET_FILE:vhdp_fixture_dyn>
          -DREADELF=${VHDP_LLVM_READELF} -DHOST_LDD=${HOST_LDD}
          -P "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/populate_dynamic.cmake"
  COMMAND gen_foreign_elf "${FIXTURE_ROOTFS}/bin/foreign"
  COMMAND ${CMAKE_COMMAND} -E touch "${FIXTURE_ROOTFS}/.stamp"
  VERBATIM)

# A minimal rootfs holding only the dynamic binary (its loader is absent).
add_custom_command(
  OUTPUT "${FIXTURE_MIN_ROOTFS}/.stamp"
  DEPENDS vhdp_fixture_dyn
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${FIXTURE_MIN_ROOTFS}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${FIXTURE_MIN_ROOTFS}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vhdp_fixture_dyn> "${FIXTURE_MIN_ROOTFS}/bin/no-loader"
  COMMAND ${CMAKE_COMMAND} -E touch "${FIXTURE_MIN_ROOTFS}/.stamp"
  VERBATIM)

add_custom_target(fixtures ALL DEPENDS "${FIXTURE_ROOTFS}/.stamp" "${FIXTURE_MIN_ROOTFS}/.stamp")

# ---- integration tests (drive phdp and libvhdp against the fixture rootfs) ----
add_executable(integration_tests
  integration/harness.cpp
  integration/test_cli_run.cpp
  integration/test_reports.cpp
  integration/test_exec.cpp
  integration/test_filesystem.cpp
  integration/test_identity.cpp
  integration/test_pty.cpp
  integration/test_signals.cpp
  integration/test_process.cpp
  integration/test_network.cpp
  integration/test_lifecycle_stress.cpp
  integration/test_library.cpp)
target_link_libraries(integration_tests PRIVATE vtest vhdp_internal vhdp_flags vhdp_warnings)
target_compile_definitions(integration_tests PRIVATE
  PHDP_BIN="$<TARGET_FILE:phdp>"
  FIXTURE_ROOTFS="${FIXTURE_ROOTFS}"
  FIXTURE_MIN_ROOTFS="${FIXTURE_MIN_ROOTFS}"
  EMBED_C_BIN="$<TARGET_FILE:embed_run_c>"
  EMBED_CPP_BIN="$<TARGET_FILE:embed_run_cpp>")
add_dependencies(integration_tests fixtures phdp embed_run_c embed_run_cpp)

set(INT_FIXTURE fixtures_ready)
vhdp_add_test(${INT_FIXTURE} COMMAND ${CMAKE_COMMAND} -E true)
set_tests_properties(${INT_FIXTURE} PROPERTIES FIXTURES_SETUP ${INT_FIXTURE})

vhdp_register_cases(integration_tests LABELS integration FIXTURES ${INT_FIXTURE} CASES
  cli_run_exit37=T-CLI-RUN-EXIT37
  cli_positional_exit37=T-CLI-POSITIONAL-EXIT37
  cli_args_env_cwd=T-CLI-ARGS-ENV-CWD
  cli_default_shell=T-PTY-DEFAULT-SHELL
  cli_doctor_json=T-CLI-DOCTOR-JSON
  cli_inspect_json=T-CLI-INSPECT-JSON
  cli_capabilities_json=T-CLI-CAPABILITIES-JSON
  cli_json_events=T-CLI-JSON-EVENTS
  cli_timeout=T-CLI-TIMEOUT
  cli_signal_forward=T-CLI-SIGNAL-FORWARD
  exec_dynamic_loader=T-EXEC-DYNAMIC-LOADER
  exec_shebang=T-EXEC-SHEBANG
  exec_abi_mismatch=T-EXEC-ABI-MISMATCH
  exec_loader_missing=T-EXEC-LOADER-MISSING
  exec_not_found=T-EXEC-NOT-FOUND
  fs_abs_rel=T-FS-ABS-REL
  fs_dirfd=T-FS-DIRFD
  fs_dotdot=T-FS-DOTDOT
  fs_symlink_escape=T-FS-SYMLINK-ESCAPE
  fs_rename_link=T-FS-RENAME-LINK
  fs_bind_ro=T-FS-BIND-RO
  fs_bind_rw=T-FS-BIND-RW
  fs_readonly_rootfs=T-FS-READONLY-ROOTFS
  fs_inherited_fd=T-FS-INHERITED-FD
  fs_unix_socket=T-FS-UNIX-SOCKET
  identity_uid_gid=T-ID-UID-GID
  dev_minimal=T-DEV-MINIMAL
  syscall_unsupported=T-SYS-UNSUPPORTED
  syscall_ptrace_only=T-SYS-PTRACE-ONLY
  net_host_socket=T-NET-HOST-SOCKET
  net_none=T-NET-NONE
  proc_fanout=T-PROC-FANOUT
  pty_ctrl_c=T-PTY-CTRL-C
  pty_resize=T-PTY-RESIZE
  pty_job_control=T-PTY-JOB-CONTROL
  lib_pty_resize=T-LIB-PTY-RESIZE
  lib_tree_cancel=T-LIFE-TREE-CANCEL
  lib_100_cycles=T-LIFE-100-CYCLES
  ex_c_embed=T-EX-C-EMBED
  ex_cpp_embed=T-EX-CPP-EMBED)

# The embedding examples link the shared library as an external (unsanitized)
# consumer would; under a sanitizer build the sanitized libvhdp cannot be loaded
# by an unsanitized executable, so skip them there (they run in every other preset).
if(VHDP_SANITIZE)
  set_tests_properties(T-EX-C-EMBED T-EX-CPP-EMBED PROPERTIES DISABLED TRUE)
endif()

# ---- ABI snapshot: exported symbols, no C++ leakage ----
# A sanitizer build injects the sanitizer runtime's exported symbols into the
# shared object, so the strict allowlist check only applies to normal builds.
if(VHDP_LLVM_NM AND NOT VHDP_SANITIZE)
  vhdp_add_test(T-ABI-SYMBOLS COMMAND ${CMAKE_COMMAND}
    -DNM=${VHDP_LLVM_NM} -DLIB=$<TARGET_FILE:vhdp>
    -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/abi/exported_symbols.txt
    -P ${CMAKE_CURRENT_SOURCE_DIR}/abi/check_symbols.cmake
    LABELS abi)
else()
  vhdp_add_test(T-ABI-SYMBOLS COMMAND ${CMAKE_COMMAND} -E echo "NOT RUN: llvm-nm/nm not found or sanitizer build")
  set_tests_properties(T-ABI-SYMBOLS PROPERTIES DISABLED TRUE)
endif()

# ---- public header self-containment (C and C++) ----
add_library(abi_header_c STATIC abi/include_c.c)
target_link_libraries(abi_header_c PRIVATE vhdp_flags)
target_include_directories(abi_header_c PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_library(abi_header_cpp STATIC abi/include_cpp.cpp)
target_link_libraries(abi_header_cpp PRIVATE vhdp_flags vhdp_warnings)
target_include_directories(abi_header_cpp PRIVATE ${PROJECT_SOURCE_DIR}/include)
vhdp_add_test(T-ABI-HEADERS-C COMMAND ${CMAKE_COMMAND} -E true)
vhdp_add_test(T-ABI-HEADERS-CPP COMMAND ${CMAKE_COMMAND} -E true)
# The compile itself is the test; register a trivial runner so CTest lists the ID.
add_dependencies(integration_tests abi_header_c abi_header_cpp)
