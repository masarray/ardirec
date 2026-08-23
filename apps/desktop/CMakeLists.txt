qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(ardirec WIN32 MACOSX_BUNDLE
    main.cpp
    document_controller.cpp
    document_controller.hpp
    analysis_controller.cpp
    analysis_controller.hpp
    waveform_item.cpp
    waveform_item.hpp
    rms_waveform_item.cpp
    rms_waveform_item.hpp
    digital_item.cpp
    digital_item.hpp
)

qt_add_qml_module(ardirec
    URI Ardirec
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/SignalSidebar.qml
        qml/TopBar.qml
        qml/ViewModeBar.qml
        qml/MeasurementPanel.qml
        qml/CursorNavigator.qml
        qml/TimeRuler.qml
        qml/TimeSignalsView.qml
        qml/TrackView.qml
        qml/EventStrip.qml
        qml/TriggerReference.qml
        qml/SectionHeader.qml
        qml/DigitalTrackView.qml
        qml/PhasorDiagram.qml
        qml/PhasorView.qml
        qml/LocusView.qml
        qml/HarmonicsView.qml
        qml/ValueTableView.qml
)

target_link_libraries(ardirec PRIVATE
    ardirec::core
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)

ardirec_set_warnings(ardirec)
ardirec_enable_sanitizers(ardirec)
