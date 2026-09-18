# libFuzzer targets for the untrusted-input parsers. Built only with a Clang that
# supports -fsanitize=fuzzer; otherwise skipped (they are run out-of-band, not by
# the default ctest suite). See docs/adr and SECURITY.md.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer-no-link")
check_cxx_source_compiles("extern \"C\" int LLVMFuzzerTestOneInput(const unsigned char*, unsigned long){return 0;}"
                          VHDP_HAVE_FUZZER)
unset(CMAKE_REQUIRED_FLAGS)

if(NOT VHDP_HAVE_FUZZER)
  message(STATUS "fuzz: libFuzzer unavailable; fuzz targets skipped")
  return()
endif()

function(vhdp_fuzzer name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE vhdp_internal)
  target_compile_options(${name} PRIVATE -g -fsanitize=fuzzer,address,undefined)
  target_link_options(${name} PRIVATE -fsanitize=fuzzer,address,undefined)
endfunction()

vhdp_fuzzer(fuzz_elf fuzz_elf.cpp)
vhdp_fuzzer(fuzz_json fuzz_json.cpp)
vhdp_fuzzer(fuzz_path fuzz_path.cpp)
vhdp_fuzzer(fuzz_shebang fuzz_shebang.cpp)
