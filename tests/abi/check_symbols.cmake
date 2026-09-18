# Verifies libvhdp.so exports exactly the C ABI allowlist and no C++/STL symbols.
# Inputs: NM (llvm-nm/nm), LIB (libvhdp.so), EXPECTED (symbol list, one per line).

execute_process(COMMAND "${NM}" -D --defined-only "${LIB}" OUTPUT_VARIABLE dump RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  # BSD/llvm nm may need a different flag; fall back.
  execute_process(COMMAND "${NM}" -D "${LIB}" OUTPUT_VARIABLE dump RESULT_VARIABLE rc)
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "nm failed on ${LIB}")
endif()

set(exported "")
string(REPLACE "\n" ";" lines "${dump}")
foreach(line IN LISTS lines)
  # Format: "<addr> <type> <name>" or " U <name>" (undefined, skipped by --defined-only).
  if(line MATCHES "[0-9a-fA-F]+ [A-TV-Ztvw] ([A-Za-z_][A-Za-z0-9_]*)")
    set(sym "${CMAKE_MATCH_1}")
    # Ignore linker-provided and versioning helper symbols.
    if(sym MATCHES "^(_init|_fini|_edata|_end|__bss_start|__.*|VHDP_[0-9]+)$")
      continue()
    endif()
    list(APPEND exported "${sym}")
  endif()
endforeach()
list(SORT exported)
list(REMOVE_DUPLICATES exported)

file(STRINGS "${EXPECTED}" expected)
list(SORT expected)

set(fail FALSE)

# Any exported symbol that is not vhdp_* is a leak (e.g. std::, __cxa_).
foreach(sym IN LISTS exported)
  if(NOT sym MATCHES "^vhdp_")
    message(WARNING "leaked non-ABI symbol exported: ${sym}")
    set(fail TRUE)
  endif()
endforeach()

# Exported vhdp_* set must equal the allowlist exactly.
foreach(sym IN LISTS expected)
  list(FIND exported "${sym}" idx)
  if(idx EQUAL -1)
    message(WARNING "expected ABI symbol missing from the library: ${sym}")
    set(fail TRUE)
  endif()
endforeach()
foreach(sym IN LISTS exported)
  if(sym MATCHES "^vhdp_")
    list(FIND expected "${sym}" idx)
    if(idx EQUAL -1)
      message(WARNING "unexpected vhdp_* symbol exported (not in allowlist): ${sym}")
      set(fail TRUE)
    endif()
  endif()
endforeach()

if(fail)
  message(FATAL_ERROR "ABI symbol check failed")
endif()
list(LENGTH expected n)
message(STATUS "ABI symbol check passed: ${n} exported C ABI symbols, no C++/STL leakage")
