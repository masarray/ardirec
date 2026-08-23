add_executable(ardirec_core_tests test_comtrade.cpp)
target_link_libraries(ardirec_core_tests PRIVATE ardirec::core)
target_compile_definitions(ardirec_core_tests PRIVATE ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
ardirec_set_warnings(ardirec_core_tests)
ardirec_enable_sanitizers(ardirec_core_tests)
add_test(NAME ardirec_core_tests COMMAND ardirec_core_tests)

add_executable(ardirec_power_tests test_power.cpp)
target_link_libraries(ardirec_power_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_power_tests)
ardirec_enable_sanitizers(ardirec_power_tests)
add_test(NAME ardirec_power_tests COMMAND ardirec_power_tests)

add_executable(ardirec_distance_tests test_distance.cpp)
target_link_libraries(ardirec_distance_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_distance_tests)
ardirec_enable_sanitizers(ardirec_distance_tests)
add_test(NAME ardirec_distance_tests COMMAND ardirec_distance_tests)

if(TARGET Qt6::Core)
    qt_add_executable(ardirec_xrio_tests
        test_xrio.cpp
        ../apps/desktop/distance_zone_controller.cpp
        ../apps/desktop/distance_zone_controller.hpp
    )
    target_include_directories(ardirec_xrio_tests PRIVATE ../apps/desktop)
    target_link_libraries(ardirec_xrio_tests PRIVATE ardirec::core Qt6::Core)
    ardirec_set_warnings(ardirec_xrio_tests)
    ardirec_enable_sanitizers(ardirec_xrio_tests)
    add_test(NAME ardirec_xrio_tests COMMAND ardirec_xrio_tests)
endif()
