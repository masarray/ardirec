add_library(ardirec_bridge SHARED
    src/ardirec_bridge.cpp
    src/ardirec_locus_bridge.cpp
)

add_library(ardirec::bridge ALIAS ardirec_bridge)

target_compile_definitions(ardirec_bridge PRIVATE ARDIREC_BRIDGE_EXPORTS)
target_include_directories(ardirec_bridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ardirec_bridge PRIVATE ardirec::core)

set_target_properties(ardirec_bridge PROPERTIES
    OUTPUT_NAME "ardirec_bridge"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

ardirec_set_warnings(ardirec_bridge)
ardirec_enable_sanitizers(ardirec_bridge)

if(ARDIREC_BUILD_TESTS)
    add_executable(ardirec_bridge_smoke tests/bridge_smoke.cpp)
    target_link_libraries(ardirec_bridge_smoke PRIVATE ardirec::bridge)
    target_compile_definitions(ardirec_bridge_smoke PRIVATE
        ARDIREC_BRIDGE_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
    )
    ardirec_set_warnings(ardirec_bridge_smoke)
    add_test(NAME ardirec_bridge_smoke COMMAND ardirec_bridge_smoke)

    add_executable(ardirec_locus_bridge_smoke tests/locus_bridge_smoke.cpp)
    target_link_libraries(ardirec_locus_bridge_smoke PRIVATE ardirec::bridge)
    target_compile_definitions(ardirec_locus_bridge_smoke PRIVATE
        ARDIREC_BRIDGE_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
    )
    ardirec_set_warnings(ardirec_locus_bridge_smoke)
    add_test(NAME ardirec_locus_bridge_smoke COMMAND ardirec_locus_bridge_smoke)
endif()
