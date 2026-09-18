# Copies a dynamic executable's interpreter and shared-library dependencies into
# ROOTFS at their original absolute paths, so the guest's own loader (resolved
# inside the rootfs by VHDP) can find them. Best-effort: on any failure a marker
# file ROOTFS/.no-dynamic is written and dynamic-exec tests skip.
# Inputs: ROOTFS, BIN, READELF, HOST_LDD (path to ldd or "").

function(copy_into_rootfs abs)
  if(NOT IS_ABSOLUTE "${abs}" OR NOT EXISTS "${abs}")
    return()
  endif()
  get_filename_component(dir "${abs}" DIRECTORY)
  file(MAKE_DIRECTORY "${ROOTFS}${dir}")
  file(COPY "${abs}" DESTINATION "${ROOTFS}${dir}" FOLLOW_SYMLINK_CHAIN)
endfunction()

set(ok TRUE)

# Interpreter (PT_INTERP).
execute_process(COMMAND "${READELF}" -l "${BIN}" OUTPUT_VARIABLE elf_out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  set(ok FALSE)
endif()
string(REGEX MATCH "interpreter: ([^]\n]+)" _m "${elf_out}")
if(CMAKE_MATCH_1)
  string(STRIP "${CMAKE_MATCH_1}" interp)
  copy_into_rootfs("${interp}")
else()
  set(ok FALSE)
endif()

# Shared libraries via ldd.
if(ok AND HOST_LDD)
  execute_process(COMMAND "${HOST_LDD}" "${BIN}" OUTPUT_VARIABLE ldd_out RESULT_VARIABLE rc2)
  if(rc2 EQUAL 0)
    string(REPLACE "\n" ";" lines "${ldd_out}")
    foreach(line IN LISTS lines)
      if(line MATCHES "=> (/[^ ]+)")
        copy_into_rootfs("${CMAKE_MATCH_1}")
      elseif(line MATCHES "^\t(/[^ ]+) ")
        copy_into_rootfs("${CMAKE_MATCH_1}")
      endif()
    endforeach()
  else()
    set(ok FALSE)
  endif()
endif()

if(NOT ok)
  file(WRITE "${ROOTFS}/.no-dynamic" "dynamic loader population failed on this host\n")
  message(STATUS "populate_dynamic: dynamic fixture unavailable; related tests will skip")
endif()
