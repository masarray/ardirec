add_executable(ardirec-cli main.cpp)
target_link_libraries(ardirec-cli PRIVATE ardirec::core)
ardirec_set_warnings(ardirec-cli)
ardirec_enable_sanitizers(ardirec-cli)
