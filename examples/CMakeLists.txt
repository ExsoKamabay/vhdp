# Embedding examples. They link the shared library through the public headers
# only, exactly as an external consumer would.
add_executable(embed_run_c c/embed_run.c)
target_link_libraries(embed_run_c PRIVATE vhdp)
target_include_directories(embed_run_c PRIVATE "${PROJECT_SOURCE_DIR}/include")

add_executable(embed_run_cpp cpp/embed_run.cpp)
target_link_libraries(embed_run_cpp PRIVATE vhdp)
target_include_directories(embed_run_cpp PRIVATE "${PROJECT_SOURCE_DIR}/include")
