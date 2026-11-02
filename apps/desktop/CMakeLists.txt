qt_standard_project_setup(REQUIRES 6.8)

file(READ "${CMAKE_SOURCE_DIR}/VERSION" ARDIREC_VERSION)
string(STRIP "${ARDIREC_VERSION}" ARDIREC_VERSION)
if(ARDIREC_VERSION STREQUAL "")
    message(FATAL_ERROR "VERSION must not be empty")
endif()

set(ARDIREC_BRAND_ICON_DIR "${CMAKE_SOURCE_DIR}/icon")

if(WIN32)
    set(ARDIREC_WINDOWS_ICON "${ARDIREC_BRAND_ICON_DIR}/favicon.ico")
    configure_file(ardirec.rc.in "${CMAKE_CURRENT_BINARY_DIR}/ardirec.rc" @ONLY)
endif()

qt_add_executable(ardirec WIN32 MACOSX_BUNDLE
    main.cpp
    document_loader.cpp
    document_loader.hpp
    document_controller.cpp
    document_controller.hpp
    analog_lod_pyramid.cpp
    analog_lod_pyramid.hpp
    analysis_controller.cpp
    analysis_controller.hpp
    analysis_cursor_batch.cpp
    analysis_sequence.cpp
    cursor_snapshot_controller.cpp
    cursor_snapshot_controller.hpp
    locus_snapshot_controller.cpp
    locus_snapshot_controller.hpp
    locus_trajectory_item.cpp
    locus_trajectory_item.hpp
    harmonic_snapshot_controller.cpp
    harmonic_snapshot_controller.hpp
    table_snapshot_controller.cpp
    table_snapshot_controller.hpp
    distance_zone_controller.cpp
    distance_zone_controller.hpp
    waveform_item.cpp
    waveform_item.hpp
    rms_cycle_window.hpp
    rms_tile_cache.cpp
    rms_tile_cache.hpp
    rms_waveform_item.cpp
    antialiased_trace_geometry.hpp
    rms_waveform_item.hpp
    phasor_vector_item.cpp
    phasor_vector_item.hpp
    phasor_vector_geometry.hpp
    digital_item.cpp
    digital_item.hpp
    $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_BINARY_DIR}/ardirec.rc>
)

qt_add_resources(ardirec ardirec_branding
    PREFIX "/branding"
    BASE "${ARDIREC_BRAND_ICON_DIR}"
    FILES
        "${ARDIREC_BRAND_ICON_DIR}/android-chrome-192x192.png"
        "${ARDIREC_BRAND_ICON_DIR}/android-chrome-512x512.png"
        "${ARDIREC_BRAND_ICON_DIR}/favicon.ico"
)

qt_add_qml_module(ardirec
    URI Ardirec
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppActions.qml
        qml/WorkstationMenuBar.qml
        qml/WorkstationStatusBar.qml
        qml/SignalSidebar.qml
        qml/TopBar.qml
        qml/ViewModeBar.qml
        qml/CursorNavigator.qml
        qml/HarmonicCursorNavigator.qml
        qml/MdiWorkspace.qml
        qml/MdiChildWindow.qml
        qml/AnalysisViewHost.qml
        qml/TimeRuler.qml
        qml/TimeSignalsView.qml
        qml/PlotGuides.qml
        qml/TrackView.qml
        qml/EventStrip.qml
        qml/TriggerReference.qml
        qml/SectionHeader.qml
        qml/DigitalTrackView.qml
        qml/DeferredEngineeringViews.qml
        qml/PhasorDiagram.qml
        qml/PhasorView.qml
        qml/SequenceSummary.qml
        qml/LocusAnalysisProxy.qml
        qml/LocusFitPolicy.qml
        qml/LocusView.qml
        qml/HarmonicDiagram.qml
        qml/HarmonicsView.qml
        qml/ValueTableView.qml
    RESOURCES
        qml/material/folder_open.svg
        qml/material/tune.svg
        qml/material/layers.svg
        qml/material/fullscreen.svg
        qml/material/my_location.svg
        qml/material/zoom_in.svg
        qml/material/zoom_out.svg
        qml/material/warning.svg
        qml/material/show_chart.svg
        qml/material/adjust.svg
        qml/material/bar_chart.svg
        qml/material/table_chart.svg
        qml/material/remove.svg
        qml/material/content_copy.svg
        qml/material/close.svg
        qml/material/delete.svg
        qml/material/visibility.svg
        qml/material/verified.svg
        qml/material/add.svg
)

target_compile_definitions(ardirec PRIVATE ARDIREC_VERSION="${ARDIREC_VERSION}")

target_link_libraries(ardirec PRIVATE
    ardirec::core
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Concurrent
)

ardirec_set_warnings(ardirec)
ardirec_enable_sanitizers(ardirec)