qt_add_executable(ardirec_r5_locus_parity_tests
    ${CMAKE_CURRENT_LIST_DIR}/test_r5_locus_parity.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/distance_zone_controller.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/distance_zone_controller.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/locus_snapshot_controller.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/locus_snapshot_controller.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/document_loader.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/document_loader.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/document_controller.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/document_controller.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/analog_lod_pyramid.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/analog_lod_pyramid.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/rms_tile_cache.cpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/rms_tile_cache.hpp
    ${CMAKE_SOURCE_DIR}/apps/desktop/rms_cycle_window.hpp
)
set_target_properties(ardirec_r5_locus_parity_tests PROPERTIES AUTOMOC ON)
target_include_directories(ardirec_r5_locus_parity_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/apps/desktop
)
target_link_libraries(ardirec_r5_locus_parity_tests PRIVATE
    ardirec::core
    Qt6::Core
    Qt6::Concurrent
    Qt6::Qml
    Qt6::Quick
)
target_compile_definitions(ardirec_r5_locus_parity_tests PRIVATE
    ARDIREC_TEST_DATA_DIR="${CMAKE_CURRENT_LIST_DIR}/data"
    ARDIREC_QML_DIR="${CMAKE_SOURCE_DIR}/apps/desktop/qml"
)
ardirec_set_warnings(ardirec_r5_locus_parity_tests)
ardirec_enable_sanitizers(ardirec_r5_locus_parity_tests)
add_test(NAME ardirec_r5_locus_parity_tests COMMAND ardirec_r5_locus_parity_tests)
set_tests_properties(ardirec_r5_locus_parity_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
