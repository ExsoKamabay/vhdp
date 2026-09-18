# Assembles a minimal fixture rootfs from already-built fixture binaries.
# Inputs: ROOTFS (output dir), STATIC_BIN, DYN_BIN (may be empty), NOLOADER_BIN (may be empty).
# The rootfs is project-owned; nothing is downloaded.

file(REMOVE_RECURSE "${ROOTFS}")
file(MAKE_DIRECTORY "${ROOTFS}/bin" "${ROOTFS}/etc" "${ROOTFS}/tmp" "${ROOTFS}/dev" "${ROOTFS}/proc"
     "${ROOTFS}/lib" "${ROOTFS}/sub/deep")

file(COPY "${STATIC_BIN}" DESTINATION "${ROOTFS}/bin")
get_filename_component(_static_name "${STATIC_BIN}" NAME)
file(RENAME "${ROOTFS}/bin/${_static_name}" "${ROOTFS}/bin/vhdp-fixture")

# Applets are argv[0] aliases (symlinks resolved inside the rootfs).
set(APPLETS sh echo cat ls exit uid stat lstat readlink try pwd printenv sleep winsize wait-winch
    ready-sleep spawn-tree fanout check-fds self-exe write chdir-pwd cwd-deleted unix-roundtrip
    bench-stat bench-getpid)
foreach(a IN LISTS APPLETS)
  file(CREATE_LINK vhdp-fixture "${ROOTFS}/bin/${a}" SYMBOLIC)
endforeach()

file(WRITE "${ROOTFS}/etc/hello.txt" "hello-content\n")
file(WRITE "${ROOTFS}/etc/os-release" "ID=vhdpfixture\nVERSION_ID=1\nPRETTY_NAME=\"VHDP Fixture 1\"\n")
file(WRITE "${ROOTFS}/sub/deep/file.txt" "deep\n")

# Symlink shapes for resolver tests (all targets stay inside the rootfs).
file(CREATE_LINK /etc/hello.txt "${ROOTFS}/etc/abs-link" SYMBOLIC)      # absolute
file(CREATE_LINK hello.txt "${ROOTFS}/etc/rel-link" SYMBOLIC)            # relative
file(CREATE_LINK /etc "${ROOTFS}/etc/dir-link" SYMBOLIC)                 # to a directory
file(CREATE_LINK loop-b "${ROOTFS}/etc/loop-a" SYMBOLIC)                 # loop
file(CREATE_LINK loop-a "${ROOTFS}/etc/loop-b" SYMBOLIC)
file(CREATE_LINK /../../../../etc/passwd "${ROOTFS}/etc/escape-link" SYMBOLIC) # escape attempt (clamped to rootfs)

if(DYN_BIN)
  file(COPY "${DYN_BIN}" DESTINATION "${ROOTFS}/bin")
  get_filename_component(_dyn "${DYN_BIN}" NAME)
  file(RENAME "${ROOTFS}/bin/${_dyn}" "${ROOTFS}/bin/vhdp-fixture-dyn")
  file(CREATE_LINK vhdp-fixture-dyn "${ROOTFS}/bin/dyn-echo" SYMBOLIC)
  file(CREATE_LINK vhdp-fixture-dyn "${ROOTFS}/bin/dyn-self-exe" SYMBOLIC)
endif()

if(NOLOADER_BIN)
  # A dynamic ELF whose PT_INTERP is a loader that does NOT exist in the rootfs.
  file(COPY "${NOLOADER_BIN}" DESTINATION "${ROOTFS}/bin")
  get_filename_component(_nl "${NOLOADER_BIN}" NAME)
  file(RENAME "${ROOTFS}/bin/${_nl}" "${ROOTFS}/bin/no-loader")
endif()

# A #! script and a foreign-architecture ELF for exec tests.
file(WRITE "${ROOTFS}/bin/script.sh" "#!/bin/echo shebang-arg\n")
file(CHMOD "${ROOTFS}/bin/script.sh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(WRITE "${ROOTFS}/bin/not-elf" "this is not an ELF or script\n")
file(CHMOD "${ROOTFS}/bin/not-elf" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
