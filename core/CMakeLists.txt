add_library(ardirec_core STATIC
    src/comtrade/parser.cpp
    src/comtrade/dat_reader.cpp
    src/comtrade/bundle.cpp
    src/comtrade/value_representation.cpp
)

add_library(ardirec::core ALIAS ardirec_core)

target_include_directories(ardirec_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

ardirec_set_warnings(ardirec_core)
ardirec_enable_sanitizers(ardirec_core)
