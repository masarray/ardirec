add_executable(ardirec-large-record-benchmark large_record_benchmark.cpp)
target_link_libraries(ardirec-large-record-benchmark PRIVATE ardirec::core)
ardirec_set_warnings(ardirec-large-record-benchmark)
