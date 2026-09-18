# Reproducible micro-benchmarks. Not part of the default ctest run; invoke the
# executable directly. Results depend on the host and are never fabricated.
add_executable(vhdp_bench bench_main.cpp)
target_link_libraries(vhdp_bench PRIVATE vhdp_static vhdp_flags)
