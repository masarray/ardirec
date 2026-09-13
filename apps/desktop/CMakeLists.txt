qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(ardirec WIN32 MACOSX_BUNDLE
    main.cpp
    document_loader.cpp
    document_loader.hpp
    document_controller.cpp
    document_controller.hpp
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
    rms_waveform_item.cpp
    rms_waveform_item.hpp
    phasor_vector_item.cpp
    phasor_vector_item.hpp
    digital_item.cpp
    digital_item.hpp
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
        qml/TimeRuler.qml
        qml/TimeSignalsView.qml
        qml/TrackView.qml
        qml/EventStrip.qml
        qml/TriggerReference.qml
        qml/SectionHeader.qml
        qml/DigitalTrackView.qml
        qml/PhasorDiagram.qml
        qml/PhasorView.qml
        qml/SequenceSummary.qml
        qml/LocusAnalysisProxy.qml
        qml/LocusView.qml
        qml/HarmonicDiagram.qml
        qml/HarmonicsView.qml
        qml/ValueTableView.qml
    RESOURCES
        qml/icons/folder-open.svg
        qml/icons/sliders-horizontal.svg
        qml/icons/maximize-2.svg
        qml/icons/crosshair.svg
        qml/icons/zoom-in.svg
        qml/icons/zoom-out.svg
        qml/icons/triangle-alert.svg
        qml/icons/activity.svg
        qml/icons/circle-dot.svg
        qml/icons/chart-column.svg
        qml/icons/table-2.svg
        qml/icons/layers-3.svg
        qml/icons/trash-2.svg
        qml/icons/eye.svg
        qml/icons/shield-check.svg
        qml/icons/minus.svg
        qml/icons/plus.svg
)

target_link_libraries(ardirec PRIVATE
    ardirec::core
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Concurrent
)

ardirec_set_warnings(ardirec)
ardirec_enable_sanitizers(ardirec)
