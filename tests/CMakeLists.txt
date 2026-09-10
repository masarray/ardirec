add_executable(ardirec_core_tests test_comtrade.cpp)
target_link_libraries(ardirec_core_tests PRIVATE ardirec::core)
target_compile_definitions(ardirec_core_tests PRIVATE ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
ardirec_set_warnings(ardirec_core_tests)
ardirec_enable_sanitizers(ardirec_core_tests)
add_test(NAME ardirec_core_tests COMMAND ardirec_core_tests)

add_executable(ardirec_lod_tests test_lod.cpp)
target_link_libraries(ardirec_lod_tests PRIVATE ardirec::core)
target_compile_definitions(ardirec_lod_tests PRIVATE ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
ardirec_set_warnings(ardirec_lod_tests)
ardirec_enable_sanitizers(ardirec_lod_tests)
add_test(NAME ardirec_lod_tests COMMAND ardirec_lod_tests)

add_executable(ardirec_field_robustness_tests test_field_robustness.cpp)
target_link_libraries(ardirec_field_robustness_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_field_robustness_tests)
ardirec_enable_sanitizers(ardirec_field_robustness_tests)
add_test(NAME ardirec_field_robustness_tests COMMAND ardirec_field_robustness_tests)

add_executable(ardirec_binary_field_robustness_tests test_binary_field_robustness.cpp)
target_link_libraries(ardirec_binary_field_robustness_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_binary_field_robustness_tests)
ardirec_enable_sanitizers(ardirec_binary_field_robustness_tests)
add_test(NAME ardirec_binary_field_robustness_tests COMMAND ardirec_binary_field_robustness_tests)

add_executable(ardirec_document_loader_tests
    test_document_loader.cpp
    ../apps/desktop/document_loader.cpp
    ../apps/desktop/document_loader.hpp
)
target_include_directories(ardirec_document_loader_tests PRIVATE ../apps/desktop)
target_link_libraries(ardirec_document_loader_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_document_loader_tests)
ardirec_enable_sanitizers(ardirec_document_loader_tests)
add_test(NAME ardirec_document_loader_tests COMMAND ardirec_document_loader_tests)

add_executable(ardirec_loader_file_safety_tests
    test_loader_file_safety.cpp
    ../apps/desktop/document_loader.cpp
    ../apps/desktop/document_loader.hpp
)
target_include_directories(ardirec_loader_file_safety_tests PRIVATE ../apps/desktop)
target_link_libraries(ardirec_loader_file_safety_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_loader_file_safety_tests)
ardirec_enable_sanitizers(ardirec_loader_file_safety_tests)
add_test(NAME ardirec_loader_file_safety_tests COMMAND ardirec_loader_file_safety_tests)

add_executable(ardirec_power_tests test_power.cpp)
target_link_libraries(ardirec_power_tests PRIVATE ardirec::core)
ardirec_set_warnings(ardirec_power_tests)
ardirec_enable_sanitizers(ardirec_power_tests)
add_test(NAME ardirec_power_tests COMMAND ardirec_power_tests)

add_executable(ardirec_distance_tests test_distance.cpp)
target_link_libraries(ardirec_distance_tests PRIVATE ardirec::core)
target_compile_definitions(ardirec_distance_tests PRIVATE ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
ardirec_set_warnings(ardirec_distance_tests)
ardirec_enable_sanitizers(ardirec_distance_tests)
add_test(NAME ardirec_distance_tests COMMAND ardirec_distance_tests)

if(TARGET Qt6::Core)
    qt_add_executable(ardirec_xrio_tests
        test_xrio.cpp
        ../apps/desktop/distance_zone_controller.cpp
        ../apps/desktop/distance_zone_controller.hpp
    )
    set_target_properties(ardirec_xrio_tests PROPERTIES AUTOMOC ON)
    target_include_directories(ardirec_xrio_tests PRIVATE ../apps/desktop)
    target_link_libraries(ardirec_xrio_tests PRIVATE ardirec::core Qt6::Core)
    ardirec_set_warnings(ardirec_xrio_tests)
    ardirec_enable_sanitizers(ardirec_xrio_tests)
    add_test(NAME ardirec_xrio_tests COMMAND ardirec_xrio_tests)

    qt_add_executable(ardirec_analysis_tests
        test_analysis.cpp
        ../apps/desktop/analysis_controller.cpp
        ../apps/desktop/analysis_cursor_batch.cpp
        ../apps/desktop/analysis_controller.hpp
        ../apps/desktop/document_loader.cpp
        ../apps/desktop/document_loader.hpp
        ../apps/desktop/document_controller.cpp
        ../apps/desktop/document_controller.hpp
    )
    set_target_properties(ardirec_analysis_tests PROPERTIES AUTOMOC ON)
    target_include_directories(ardirec_analysis_tests PRIVATE ../apps/desktop)
    target_link_libraries(ardirec_analysis_tests PRIVATE ardirec::core Qt6::Core Qt6::Concurrent)
    target_compile_definitions(ardirec_analysis_tests PRIVATE ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    ardirec_set_warnings(ardirec_analysis_tests)
    ardirec_enable_sanitizers(ardirec_analysis_tests)
    add_test(NAME ardirec_analysis_tests COMMAND ardirec_analysis_tests)
endif()
